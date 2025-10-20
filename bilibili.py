from functools import reduce
from hashlib import md5
import urllib.parse
import time
import requests
import os
import json
import logging

mixinKeyEncTab = [
    46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35, 27, 43, 5, 49,
    33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13, 37, 48, 7, 16, 24, 55, 40,
    61, 26, 17, 0, 1, 60, 51, 30, 4, 22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11,
    36, 20, 34, 44, 52
]

def getMixinKey(orig: str):
    '对 imgKey 和 subKey 进行字符顺序打乱编码'
    return reduce(lambda s, i: s + orig[i], mixinKeyEncTab, '')[:32]

def encWbi(params: dict, img_key: str, sub_key: str):
    '为请求参数进行 wbi 签名'
    mixin_key = getMixinKey(img_key + sub_key)
    curr_time = round(time.time())
    params['wts'] = curr_time                                   # 添加 wts 字段
    params = dict(sorted(params.items()))                       # 按照 key 重排参数
    # 过滤 value 中的 "!'()*" 字符
    params = {
        k : ''.join(filter(lambda chr: chr not in "!'()*", str(v)))
        for k, v 
        in params.items()
    }
    query = urllib.parse.urlencode(params, quote_via=urllib.parse.quote)                      # 序列化参数
    print("[encWbi] query = "+ query)
    wbi_sign = md5((query + mixin_key).encode()).hexdigest()    # 计算 w_rid
    params['w_rid'] = wbi_sign
    return params

def getWbiKeys(headers: dict = None):
    resp = requests.get('https://api.bilibili.com/x/web-interface/nav', headers=headers)
    resp.raise_for_status()
    json_content = resp.json()
    img_url: str = json_content['data']['wbi_img']['img_url']
    sub_url: str = json_content['data']['wbi_img']['sub_url']
    img_key = img_url.rsplit('/', 1)[1].split('.')[0]
    sub_key = sub_url.rsplit('/', 1)[1].split('.')[0]
    return img_key, sub_key

class BiliDownloader:
    def __init__(self, cfg=None, logger:logging.Logger=None) -> None:
        if logger is not None:
            self.logger = logger
        else:
            self.logger = logging.getLogger()
        if cfg is not None and os.path.exists(cfg):
            self.cfg = cfg
            with open(cfg, 'r') as f:
                user_info = json.load(f)
                self.requestHeader = user_info['header']
                self.cookies = user_info['cookies']
                self.img_key = user_info['img_key']
                self.sub_key = user_info['sub_key']
                self.last_update = user_info['last_update']
        else:
            self.cfg = cfg if cfg is not None else 'userinfo.json'
            self.requestHeader = { 
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
                '(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36',
                'Accept': 'application/json, text/plain, */*',
                'Accept-Language': 'zh-CN,zh;q=0.9',
                'Connection': 'keep-alive',
                'Content-Type': 'application/x-www-form-urlencoded',
                "Referer": "https://www.bilibili.com"
            }
            self.flushConfig({
                'header': self.requestHeader
            })
            self.cookies = {}
            self.img_key = ''
            self.sub_key = ''
            self.last_update = 0
        self.check_agent()
        pass
    
    def check_agent(self):
        if len(self.cookies.items()) == 0:
            # Access the website and save cookies
            response = requests.get('https://www.bilibili.com', headers=self.requestHeader)
            cookies = response.cookies
            #convert cookies from RequestsCookieJar to dict
            self.cookies = requests.utils.dict_from_cookiejar(cookies)
            self.flushConfig({
                'cookies': self.cookies
            })

            # check if img_key and sub_key is empty and now time to last_update(timestamp) is more than 1 day
        if self.img_key == '' or self.sub_key == '' or (time.time() - self.last_update) > 86400:
            self.img_key, self.sub_key = getWbiKeys(self.requestHeader)
            self.last_update = time.time()
            self.flushConfig({
                'img_key': self.img_key,
                'sub_key': self.sub_key,
                'last_update': self.last_update
            })

    def flushConfig(self, kv:dict):
        user_info = {}
        if os.path.exists(self.cfg):
            with open(self.cfg, 'r') as f:
                user_info = json.load(f)
        for k, v in kv.items():
            user_info[k] = v
        with open(self.cfg, 'w') as f:
            json.dump(user_info, f, indent=4)
            
    def search_by_title(self, title)->list:
        signed_params = encWbi(
            params={
                'search_type': 'video',
                'keyword': title,
                'page':1,
            },
            img_key=self.img_key,
            sub_key=self.sub_key
        )
        # Make HTTP GET request to https://api.bilibili.com/x/web-interface/wbi/search/all/v2
        params = urllib.parse.urlencode(signed_params, quote_via=urllib.parse.quote)
        print("params: "+params)
        response = requests.get('https://api.bilibili.com/x/web-interface/wbi/search/type', 
                                headers=self.requestHeader, 
                                params=params, 
                                cookies=self.cookies)
        #print request url
        print(f'Request URL: {response.url}')
        if response.status_code != 200:
            self.logger.error(f'Failed to get search result, for HTTP GET request failed. Status code: {response.status_code}')
            return []
    
        self.cookies.update(response.cookies)
        self.flushConfig({
                'cookies': self.cookies
            })
        resp_json= response.json()
        with open('bili/bvid_resp.json', 'w') as f:
            json.dump(resp_json, f, indent=4)
        results = resp_json["data"]["result"]
        return list(map(lambda result: {
            'title': result['title'].replace('<em class="keyword">', '').replace('</em>', ''),
            'bvid': result['bvid'],
            'author': result['author'],
            'description': result['description']
        }, results))
    
    def get_pages_cid(self, bvid)->list:
        response = requests.get(url='https://api.bilibili.com/x/player/pagelist',
                                headers=self.requestHeader,
                                params={'bvid': bvid},
                                cookies=self.cookies)
        if response.status_code != 200:
            self.logger.error(f'Failed to get bv info, for HTTP GET request failed. Status code: {response.status_code}')
            return ""

        self.cookies.update(response.cookies)
        resp_json = response.json()
        _code = resp_json["code"]
        if _code !=0:
            _err_msg = resp_json["message"]
            self.logger.error(f'Failed to get bv info, for code is not 0. Code: {_code}, Message: {_err_msg}')
            return []
        
        _pages = resp_json["data"]
        with open('bili/cid_resp.json', 'w') as f:
            json.dump(resp_json, f, indent=4)
        page = []
        for _page in _pages:
            page.append({
                'cid': _page["cid"],
                'page': _page["page"],
                'part': _page["part"],
                'duration': _page["duration"],
                # 'first_frame': _page["first_frame"],
            })
        return page
        
    
    def get_audio_link(self, bvid, cid)->str:
        signed_params = encWbi(
            params={
                'bvid': bvid,
                'cid': cid,
                'fnval':16,
                'qn': 64,
            },
            img_key=self.img_key,
            sub_key=self.sub_key
        )

        response = requests.get(url='https://api.bilibili.com/x/player/wbi/playurl',
                                headers=self.requestHeader,
                                params=signed_params,
                                cookies=self.cookies)
        if response.status_code != 200:
            self.logger.error(f'Failed to get audio info, for HTTP GET request failed. Status code: {response.status_code}')
            return ""

        self.cookies.update(response.cookies)
        self.flushConfig({
                'cookies': self.cookies
            })
        resp_json = response.json()
        dash_list = sorted(resp_json["data"]["dash"]["audio"], key=lambda x:x["bandwidth"],reverse=True)
        if len(dash_list)<=0:
            self.logger.error(f'Failed to get audio info, for audio list is empty.')
            return ""
        return dash_list[0]["baseUrl"]
    
    def get_favorites(self, fid:int, page:int=1, ps:int=20):
        signed_params = encWbi(
            params={
                'media_id': fid,
                'ps': ps,
                'pn': page,
            },
            img_key=self.img_key,
            sub_key=self.sub_key
        )

        response = requests.get(url='https://api.bilibili.com/x/v3/fav/resource/list',
                                headers=self.requestHeader,
                                params=signed_params,
                                cookies=self.cookies)
        if response.status_code != 200:
            self.logger.error(f'Failed to get audio info, for HTTP GET request failed. Status code: {response.status_code}')
            return ""

        self.cookies.update(response.cookies)
        self.flushConfig({
                'cookies': self.cookies
            })
        resp_json = response.json()
        if resp_json["code"] != 0:
            self.logger.info(f'Failed to get favorites info, for code is not 0. Code: {resp_json["code"]}, Message: {resp_json["message"]}')
            return None
        return resp_json["data"]

    def get_user_favs(self, uid:int):
        signed_params = encWbi(
            params={
                'up_mid': uid,
            },
            img_key=self.img_key,
            sub_key=self.sub_key
        )

        response = requests.get(url='https://api.bilibili.com/x/v3/fav/folder/created/list-all',
                                headers=self.requestHeader,
                                params=signed_params,
                                cookies=self.cookies)
        if response.status_code != 200:
            self.logger.error(f'Failed to get audio info, for HTTP GET request failed. Status code: {response.status_code}')
            return ""

        self.cookies.update(response.cookies)
        self.flushConfig({
                'cookies': self.cookies
            })
        resp_json = response.json()
        if resp_json["code"] != 0:
            self.logger.info(f'Failed to get favorites info, for code is not 0. Code: {resp_json["code"]}, Message: {resp_json["message"]}')
            return None
        return resp_json["data"]