#pragma once

#include <atomic>
#include <QString>
#include <memory>

namespace network
{
    /**
     * @brief Represents an in-progress async search operation
     * 
     * Allows cancellation of long-running search operations via the `cancelled` flag.
     * This is used to support rapid navigation - when user starts a new search while
     * one is in progress, the old one can be cancelled to avoid processing stale results.
     * 
     * Thread-safe: The `cancelled` flag uses std::atomic<bool> for lock-free access
     * from both the search thread and the main UI thread.
     */
    struct AsyncSearchOperation
    {
        /// Unique search request identifier (keyword + timestamp, etc.)
        QString searchId;
        
        /// Atomic flag set to true when search should be cancelled
        /// - Main UI thread: sets to true when user initiates new search
        /// - Search thread: checks periodically and stops processing when true
        std::atomic<bool> cancelled{false};
        
        /// Associated keyword being searched
        QString keyword;
        
        AsyncSearchOperation() = default;
        
        explicit AsyncSearchOperation(const QString& id, const QString& kw)
            : searchId(id), keyword(kw), cancelled(false)
        {
        }
        
        /// Check if this operation should be cancelled
        bool isCancelled() const
        {
            return cancelled.load(std::memory_order_acquire);
        }
        
        /// Request cancellation of this operation
        void requestCancel()
        {
            cancelled.store(true, std::memory_order_release);
        }
    };
    
    /// Shared ownership of AsyncSearchOperation
    using AsyncSearchOpPtr = std::shared_ptr<AsyncSearchOperation>;

}  // namespace network
