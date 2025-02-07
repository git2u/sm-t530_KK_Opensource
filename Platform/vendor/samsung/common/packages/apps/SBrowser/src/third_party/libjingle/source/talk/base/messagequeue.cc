/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef POSIX
#include <sys/time.h>
#endif

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/messagequeue.h"
#include "talk/base/physicalsocketserver.h"


namespace talk_base {

const uint32 kMaxMsgLatency = 150;  // 150 ms

//------------------------------------------------------------------
// MessageQueueManager

MessageQueueManager* MessageQueueManager::instance_;

MessageQueueManager* MessageQueueManager::Instance() {
  // Note: This is not thread safe, but it is first called before threads are
  // spawned.
  if (!instance_)
    instance_ = new MessageQueueManager;
  return instance_;
}

MessageQueueManager::MessageQueueManager() {
}

MessageQueueManager::~MessageQueueManager() {
}

void MessageQueueManager::Add(MessageQueue *message_queue) {
  // MessageQueueManager methods should be non-reentrant, so we
  // ASSERT that is the case.  If any of these ASSERT, please
  // contact bpm or jbeda.
  ASSERT(!crit_.CurrentThreadIsOwner());
  CritScope cs(&crit_);
  message_queues_.push_back(message_queue);
}

void MessageQueueManager::Remove(MessageQueue *message_queue) {
  ASSERT(!crit_.CurrentThreadIsOwner());  // See note above.
  // If this is the last MessageQueue, destroy the manager as well so that
  // we don't leak this object at program shutdown. As mentioned above, this is
  // not thread-safe, but this should only happen at program termination (when
  // the ThreadManager is destroyed, and threads are no longer active).
  bool destroy = false;
  {
    CritScope cs(&crit_);
    std::vector<MessageQueue *>::iterator iter;
    iter = std::find(message_queues_.begin(), message_queues_.end(),
                     message_queue);
    if (iter != message_queues_.end()) {
      message_queues_.erase(iter);
    }
    destroy = message_queues_.empty();
  }
  if (destroy) {
    instance_ = NULL;
    delete this;
  }
}

void MessageQueueManager::Clear(MessageHandler *handler) {
  ASSERT(!crit_.CurrentThreadIsOwner());  // See note above.
  CritScope cs(&crit_);
  std::vector<MessageQueue *>::iterator iter;
  for (iter = message_queues_.begin(); iter != message_queues_.end(); iter++)
    (*iter)->Clear(handler);
}

//------------------------------------------------------------------
// MessageQueue

MessageQueue::MessageQueue(SocketServer* ss)
    : ss_(ss), fStop_(false), fPeekKeep_(false), active_(false),
      dmsgq_next_num_(0) {
  if (!ss_) {
    // Currently, MessageQueue holds a socket server, and is the base class for
    // Thread.  It seems like it makes more sense for Thread to hold the socket
    // server, and provide it to the MessageQueue, since the Thread controls
    // the I/O model, and MQ is agnostic to those details.  Anyway, this causes
    // messagequeue_unittest to depend on network libraries... yuck.
    default_ss_.reset(new PhysicalSocketServer());
    ss_ = default_ss_.get();
  }
  ss_->SetMessageQueue(this);
}

MessageQueue::~MessageQueue() {
  // The signal is done from here to ensure
  // that it always gets called when the queue
  // is going away.
  SignalQueueDestroyed();
  if (active_) {
    MessageQueueManager::Instance()->Remove(this);
    Clear(NULL);
  }
  if (ss_) {
    ss_->SetMessageQueue(NULL);
  }
}

void MessageQueue::set_socketserver(SocketServer* ss) {
  ss_ = ss ? ss : default_ss_.get();
  ss_->SetMessageQueue(this);
}

void MessageQueue::Quit() {
  fStop_ = true;
  ss_->WakeUp();
}

bool MessageQueue::IsQuitting() {
  return fStop_;
}

void MessageQueue::Restart() {
  fStop_ = false;
}

bool MessageQueue::Peek(Message *pmsg, int cmsWait) {
  if (fPeekKeep_) {
    *pmsg = msgPeek_;
    return true;
  }
  if (!Get(pmsg, cmsWait))
    return false;
  msgPeek_ = *pmsg;
  fPeekKeep_ = true;
  return true;
}

struct MessageDeleter {
  ~MessageDeleter() {
    while (!messages.empty()) {
      ASSERT(NULL == messages.front().phandler);
      delete messages.front().pdata;
      messages.pop_front();
    }
  }
  MessageList messages;
};

bool MessageQueue::Get(Message *pmsg, int cmsWait, bool process_io) {
  // Return and clear peek if present
  // Always return the peek if it exists so there is Peek/Get symmetry

  if (fPeekKeep_) {
    *pmsg = msgPeek_;
    fPeekKeep_ = false;
    return true;
  }

  // Get w/wait + timer scan / dispatch + socket / event multiplexer dispatch

  int cmsTotal = cmsWait;
  int cmsElapsed = 0;
  uint32 msStart = Time();
  uint32 msCurrent = msStart;
  while (true) {
    // Check for sent messages

    ReceiveSends();

    // Check queues

    int cmsDelayNext = kForever;
    {
      MessageDeleter deleter;
      CritScope cs(&crit_);

      // Check for delayed messages that have been triggered
      // Calc the next trigger too

      while (!dmsgq_.empty()) {
        if (TimeIsLater(msCurrent, dmsgq_.top().msTrigger_)) {
          cmsDelayNext = TimeDiff(dmsgq_.top().msTrigger_, msCurrent);
          break;
        }
        msgq_.push_back(dmsgq_.top().msg_);
        dmsgq_.pop();
      }

      // Check for posted events

      while (!msgq_.empty()) {
        *pmsg = msgq_.front();
        if (pmsg->ts_sensitive) {
          long delay = TimeDiff(msCurrent, pmsg->ts_sensitive);
          if (delay > 0) {
            LOG_F(LS_WARNING) << "id: " << pmsg->message_id << "  delay: "
                              << (delay + kMaxMsgLatency) << "ms";
          }
        }
        msgq_.pop_front();
        if (MQID_DISPOSE == pmsg->message_id) {
          // Delete the object, but *not inside the crit scope!*.
          deleter.messages.push_back(*pmsg);
          // To be safe, make sure we don't return this message.
          *pmsg = Message();
          continue;
        }
        return true;
      }
    }

    if (fStop_)
      break;

    // Which is shorter, the delay wait or the asked wait?

    int cmsNext;
    if (cmsWait == kForever) {
      cmsNext = cmsDelayNext;
    } else {
      cmsNext = _max(0, cmsTotal - cmsElapsed);
      if ((cmsDelayNext != kForever) && (cmsDelayNext < cmsNext))
        cmsNext = cmsDelayNext;
    }

    // Wait and multiplex in the meantime
    if (!ss_->Wait(cmsNext, process_io))
      return false;

    // If the specified timeout expired, return

    msCurrent = Time();
    cmsElapsed = TimeDiff(msCurrent, msStart);
    if (cmsWait != kForever) {
      if (cmsElapsed >= cmsWait)
        return false;
    }
  }
  return false;
}

void MessageQueue::ReceiveSends() {
}

void MessageQueue::Post(MessageHandler *phandler, uint32 id,
    MessageData *pdata, bool time_sensitive) {
  if (fStop_)
    return;

  // Keep thread safe
  // Add the message to the end of the queue
  // Signal for the multiplexer to return

  CritScope cs(&crit_);
  EnsureActive();
  Message msg;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  if (time_sensitive) {
    msg.ts_sensitive = Time() + kMaxMsgLatency;
  }
  msgq_.push_back(msg);
  ss_->WakeUp();
}

void MessageQueue::DoDelayPost(int cmsDelay, uint32 tstamp,
    MessageHandler *phandler, uint32 id, MessageData* pdata) {
  if (fStop_)
    return;

  // Keep thread safe
  // Add to the priority queue. Gets sorted soonest first.
  // Signal for the multiplexer to return.

  CritScope cs(&crit_);
  EnsureActive();
  Message msg;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  DelayedMessage dmsg(cmsDelay, tstamp, dmsgq_next_num_, msg);
  dmsgq_.push(dmsg);
  // If this message queue processes 1 message every millisecond for 50 days,
  // we will wrap this number.  Even then, only messages with identical times
  // will be misordered, and then only briefly.  This is probably ok.
  VERIFY(0 != ++dmsgq_next_num_);
  ss_->WakeUp();
}

int MessageQueue::GetDelay() {
  CritScope cs(&crit_);

  if (!msgq_.empty())
    return 0;

  if (!dmsgq_.empty()) {
    int delay = TimeUntil(dmsgq_.top().msTrigger_);
    if (delay < 0)
      delay = 0;
    return delay;
  }

  return kForever;
}

void MessageQueue::Clear(MessageHandler *phandler, uint32 id,
                         MessageList* removed) {
  CritScope cs(&crit_);

  // Remove messages with phandler

  if (fPeekKeep_ && msgPeek_.Match(phandler, id)) {
    if (removed) {
      removed->push_back(msgPeek_);
    } else {
      delete msgPeek_.pdata;
    }
    fPeekKeep_ = false;
  }

  // Remove from ordered message queue

  for (MessageList::iterator it = msgq_.begin(); it != msgq_.end();) {
    if (it->Match(phandler, id)) {
      if (removed) {
        removed->push_back(*it);
      } else {
        delete it->pdata;
      }
      it = msgq_.erase(it);
    } else {
      ++it;
    }
  }

  // Remove from priority queue. Not directly iterable, so use this approach

  PriorityQueue::container_type::iterator new_end = dmsgq_.container().begin();
  for (PriorityQueue::container_type::iterator it = new_end;
       it != dmsgq_.container().end(); ++it) {
    if (it->msg_.Match(phandler, id)) {
      if (removed) {
        removed->push_back(it->msg_);
      } else {
        delete it->msg_.pdata;
      }
    } else {
      *new_end++ = *it;
    }
  }
  dmsgq_.container().erase(new_end, dmsgq_.container().end());
  dmsgq_.reheap();
}

void MessageQueue::Dispatch(Message *pmsg) {
  pmsg->phandler->OnMessage(pmsg);
}

void MessageQueue::EnsureActive() {
  ASSERT(crit_.CurrentThreadIsOwner());
  if (!active_) {
    active_ = true;
    MessageQueueManager::Instance()->Add(this);
  }
}

}  // namespace talk_base
