
#include <event/event_bus.hpp>
#include <cstdlib>
#include <fmt/format.h>
#include <memory>
#include <iostream>

#ifdef CXX11
using namespace std::placeholders;
#else
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/bind/placeholders.hpp>
using namespace boost;  
#endif
#include <catch2/catch_test_macros.hpp>

struct IntEvent
{
    int data;
};

struct StrEvent
{
    std::string data;
};

class ISEvent
{
public:
    int data;
    std::string data2;
};

class PrintSubscriber : public enable_shared_from_this<PrintSubscriber>
{
public:
    int id;
    int check;
    explicit PrintSubscriber() : id(0), check(0){}
    virtual ~PrintSubscriber()
    {
        // LOG_INFO("PrintSubscriber %s destroyed.", id);
    }
    bool OnDemoEvent(const IntEvent& event)
    {
        check = event.data;
        return true;
    }
};



class MultiPrinterSubscriber : public enable_shared_from_this<MultiPrinterSubscriber>
{
public:
    int id;
    int iCheck;
    std::string strCheck;
    explicit MultiPrinterSubscriber() : id(0), iCheck(0), strCheck(""){}
    virtual ~MultiPrinterSubscriber()
    {
        std::cout << "MultiPrinterSubscriber " << id << " destroyed." << std::endl;
    }
    bool OnDemoEvent2(const StrEvent& event)
    {
        strCheck = event.data;
        return true;
    }
    bool OnDemoEvent3(const ISEvent& event)
    {
        iCheck = event.data;
        strCheck = event.data2;
        return true;
    }
};

class BenchmarkSubscriber : public enable_shared_from_this<BenchmarkSubscriber>
{
public:
    int id;
    int iCheck;
    explicit BenchmarkSubscriber() : id(0), iCheck(0){}
    virtual ~BenchmarkSubscriber()
    {
        // LOG_INFO("BenchmarkSubscriber %d destroyed.", id);
    }
    int gcd(int a, int b)
    {
        if (b == 0)
            return a;
        return gcd(b, a % b);
    }
    bool OnDemoEvent(const IntEvent& event)
    {
        int iGcd = gcd(id, event.data);
        iCheck = (id/iGcd) * (event.data/iGcd) * iGcd;
        return true;
    }
};

TEST_CASE("EventHandler Test", "[EventHandler]")
{
    srand(time(NULL));
    SECTION("BasicTest")
    {
        shared_ptr<EventBus> eventBus1 = EventBus::Create();
        shared_ptr<PrintSubscriber> subscriber = make_shared<PrintSubscriber>();
        subscriber->id = 1;
        eventBus1->Subscribe<IntEvent>(subscriber->shared_from_this(), bind(&PrintSubscriber::OnDemoEvent, subscriber.get(), _1));
        int check = rand();
        IntEvent event;
        event.data = check;
        REQUIRE(eventBus1->Post(event));
        REQUIRE(subscriber->check == check);
    }
    SECTION("MultiTest")
    {
        shared_ptr<EventBus> eventBus = EventBus::Create();
        shared_ptr<MultiPrinterSubscriber> subscriber = make_shared<MultiPrinterSubscriber>();
        subscriber->id = 2;
        eventBus->Subscribe<StrEvent>(subscriber->shared_from_this(), bind(&MultiPrinterSubscriber::OnDemoEvent2, subscriber.get(), _1));
        eventBus->Subscribe<ISEvent>(subscriber->shared_from_this(), bind(&MultiPrinterSubscriber::OnDemoEvent3, subscriber.get(), _1));
        int iCheck = rand();
        std::string strCheck = fmt::to_string(iCheck);
        StrEvent event;
        event.data = strCheck;
        REQUIRE(eventBus->Post(event));
        REQUIRE(subscriber->strCheck == strCheck);
        iCheck = rand();
        strCheck = fmt::to_string(iCheck);
        ISEvent event3;
        event3.data = iCheck;
        event3.data2 = strCheck;
        REQUIRE(eventBus->Post(event3));
        REQUIRE(subscriber->iCheck == iCheck);
        REQUIRE(subscriber->strCheck == strCheck);
    }
    SECTION("Benchmark")
    {
        shared_ptr<EventBus> eventBus = EventBus::Create();
        std::vector<shared_ptr<BenchmarkSubscriber> > subscribers;
        for(int i=3;i<1000000;i++)
        {
            shared_ptr<BenchmarkSubscriber> subscriber = make_shared<BenchmarkSubscriber>();
            subscriber->id = i;
            eventBus->Subscribe<IntEvent>(subscriber->shared_from_this(), bind(&BenchmarkSubscriber::OnDemoEvent, subscriber.get(), _1));
            subscribers.push_back(subscriber);
            if (i%1000 == 0)
            {
                // LOG_INFO("Subscribers: %d/1,000,000", i);
            }
        }
        int iCheck = rand();
        IntEvent event;
        event.data = iCheck;
        REQUIRE(eventBus->Post(event));
        bool success = true;
        for(std::vector<shared_ptr<BenchmarkSubscriber> >::iterator it = subscribers.begin(); it!=subscribers.end();it++)
        {
            shared_ptr<BenchmarkSubscriber> subscriber = *it;
            //check if subscriber.check is the lcs of iCheck and subscriber.id
            int iGcd = subscriber->gcd(iCheck, subscriber->id);
            if (subscriber->iCheck != (subscriber->id/iGcd) * (iCheck/iGcd) * iGcd)
            {
                std::cerr << "Subscriber " << subscriber->id << " failed, check is " << iCheck << ", lcs is " << subscriber->iCheck << "." << std::endl;
                success = false;
                break;
            }
            REQUIRE(success);
        }
        std::cout << "Benchmark test pass." << std::endl;
        shared_ptr<BenchmarkSubscriber> cleanTrigger = make_shared<BenchmarkSubscriber>();
        cleanTrigger->id = 1000000;
        eventBus->Subscribe<IntEvent>(cleanTrigger->shared_from_this(), bind(&BenchmarkSubscriber::OnDemoEvent, cleanTrigger.get(), _1));
        event.data = 0;
        REQUIRE(eventBus->Post(event));
    }
}