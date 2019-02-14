/*
 *      Copyright (C) 2014-2016 Jean-Luc Barriere
 *
 *  This file is part of Noson
 *
 *  Noson is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Noson is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef EVENTHANDLER_H
#define	EVENTHANDLER_H

#include "local_config.h"
#include "sharedptr.h"
#include "requestbroker.h"
#include "locked.h"

#include <string>
#include <vector>

#define EVENTHANDLER_STARTED        "STARTED"
#define EVENTHANDLER_STOPPED        "STOPPED"
#define EVENTHANDLER_FAILED         "FAILED"
#define EVENTHANDLER_TIMEOUT        1 // 1 sec

namespace NSROOT
{
  typedef enum
  {
    EVENT_HANDLER_STATUS = 0,     // Internal event: Backend status change
    EVENT_HANDLER_TIMER,          // Internal event: Every cycle without event
    EVENT_UPNP_PROPCHANGE,        // upnp:propchange
    EVENT_UNKNOWN,
  } EVENT_t;

  struct EventMessage
  {
    EVENT_t                   event;
    std::vector<std::string>  subject;

    EventMessage()
    : event(EVENT_UNKNOWN)
    {}
  };

  typedef SHARED_PTR<EventMessage> EventMessagePtr;

  class EventSubscriber
  {
  public:
    virtual ~EventSubscriber() {}
    virtual void HandleEventMessage(EventMessagePtr msg) = 0;
  };

  class EventHandler
  {
  public:
    EventHandler();
    EventHandler(unsigned bindingPort);

    bool Start() { return m_imp ? m_imp->Start(): false; }
    void Stop() { if (m_imp) m_imp->Stop(); }
    std::string GetAddress() const { return m_imp ? m_imp->GetAddress() : ""; }
    unsigned GetPort() const { return m_imp ? m_imp->GetPort(): 0; }
    bool IsRunning() { return m_imp ? m_imp->IsRunning() : false; }

    void RegisterRequestBroker(RequestBrokerPtr rb) { if (m_imp) m_imp->RegisterRequestBroker(rb); }
    void UnregisterRequestBroker(const std::string& name) { if (m_imp) m_imp->UnregisterRequestBroker(name); }
    RequestBrokerPtr GetRequestBroker(const std::string& name) { return m_imp ? m_imp->GetRequestBroker(name) : RequestBrokerPtr(); }
    std::vector<RequestBrokerPtr> AllRequestBroker() { return m_imp ? m_imp->AllRequestBroker() : std::vector<RequestBrokerPtr>(); }

    unsigned CreateSubscription(EventSubscriber *sub) { return m_imp ? m_imp->CreateSubscription(sub) : 0; }
    bool SubscribeForEvent(unsigned subid, EVENT_t event) { return m_imp ? m_imp->SubscribeForEvent(subid, event) : false; }
    void RevokeSubscription(unsigned subid) { if (m_imp) m_imp->RevokeSubscription(subid); }
    void RevokeAllSubscriptions(EventSubscriber *sub) { if (m_imp) m_imp->RevokeAllSubscriptions(sub); }

    class EventHandlerThread
    {
      friend class EventHandler;
    public:
      EventHandlerThread(unsigned bindingPort);
      virtual ~EventHandlerThread();
      std::string GetAddress() const { return m_listenerAddress; }
      unsigned GetPort() const { return m_port; }
      virtual bool Start() = 0;
      virtual void Stop() = 0;
      virtual bool IsRunning() = 0;
      virtual unsigned CreateSubscription(EventSubscriber *sub) = 0;
      virtual bool SubscribeForEvent(unsigned subid, EVENT_t event) = 0;
      virtual void RevokeSubscription(unsigned subid) = 0;
      virtual void RevokeAllSubscriptions(EventSubscriber *sub) = 0;
      virtual void DispatchEvent(const EventMessage& msg) = 0;

      /**
       * @brief Configure a callback to handle any other requests than supported by the event broker.
       * @param rb the pointer to the request broker instance or null
       */
      virtual void RegisterRequestBroker(RequestBrokerPtr rb) = 0;
      virtual void UnregisterRequestBroker(const std::string& name) = 0;
      virtual RequestBrokerPtr GetRequestBroker(const std::string& name) = 0;
      virtual std::vector<RequestBrokerPtr> AllRequestBroker() = 0;

    protected:
      std::string m_listenerAddress;
      unsigned m_port;
    };

    typedef SHARED_PTR<EventHandlerThread> EventHandlerThreadPtr;

  private:
    EventHandlerThreadPtr m_imp;
  };

}

#endif	/* EVENTHANDLER_H */
