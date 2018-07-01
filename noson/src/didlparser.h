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

#ifndef DIDLPARSER_H
#define	DIDLPARSER_H

#include "local_config.h"
#include "digitalitem.h"

#include <vector>

#define DIDL_XMLNS_DC   "http://purl.org/dc/elements/1.1/"
#define DIDL_XMLNS_UPNP "urn:schemas-upnp-org:metadata-1-0/upnp/"
#define DIDL_XMLNS_RINC "urn:schemas-rinconnetworks-com:metadata-1-0/"
#define DIDL_XMLNS_DIDL "urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"

#define DIDL_QNAME_DC   "dc:"
#define DIDL_QNAME_UPNP "upnp:"
#define DIDL_QNAME_RINC "r:"
#define DIDL_QNAME_DIDL ""

namespace NSROOT
{
  class DIDLParser
  {
  public:
    DIDLParser(const char* document, unsigned reserve = 0);
    virtual ~DIDLParser() {}

    bool IsValid() { return m_parsed; }

    std::vector<DigitalItemPtr>& GetItems() { return m_items; }

    static const char* KeyForNameSpace(const char* name);
    static const char* DIDLNSString();

  private:
    const char* m_document;
    bool m_parsed;

    std::vector<DigitalItemPtr> m_items;

    bool Parse();

  };
}


#endif	/* DIDLPARSER_H */

