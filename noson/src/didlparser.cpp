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

#include "didlparser.h"
#include "private/tinyxml2.h"
#include "private/xmldict.h"
#include "private/debug.h"
#include "private/cppdef.h"

using namespace NSROOT;

namespace NSROOT
{
  XMLDict __initDIDLDict()
  {
    XMLDict dict;
    dict.DefineNS(DIDL_QNAME_DIDL, DIDL_XMLNS_DIDL);
    dict.DefineNS(DIDL_QNAME_RINC, DIDL_XMLNS_RINC);
    dict.DefineNS(DIDL_QNAME_DC, DIDL_XMLNS_DC);
    dict.DefineNS(DIDL_QNAME_UPNP, DIDL_XMLNS_UPNP);
    return dict;
  }
  XMLDict DIDLDict = __initDIDLDict();
}

DIDLParser::DIDLParser(const char* document, unsigned reserve)
: m_document(document)
, m_parsed(false)
{
  if (reserve)
    m_items.reserve(reserve);
  m_parsed = Parse();
}

const char* DIDLParser::KeyForNameSpace(const char* name)
{
  return DIDLDict.KeyForName(name);
}

const char* DIDLParser::DIDLNSString()
{
  return DIDLDict.ToString();
}

bool DIDLParser::Parse()
{
  m_items.clear();
  tinyxml2::XMLDocument doc;
  if (doc.Parse(m_document) != tinyxml2::XML_SUCCESS)
    return false;
  const tinyxml2::XMLElement* elem;
  if ((elem = doc.RootElement()) && XMLNS::NameEqual(elem->Name(), "DIDL-Lite"))
  {
    // learn declared namespaces in the element the DIDL-Lite for translations
    XMLNames xmlnames;
    xmlnames.AddXMLNS(elem);
    // loop over elements
    elem = elem->FirstChildElement();
    while (elem)
    {
      struct { const char* id; const char* parentID; bool restricted; } ref;
      if ((XMLNS::NameEqual(elem->Name(), "item") ||
              XMLNS::NameEqual(elem->Name(), "container")))
      {
        const char* val;
        val = elem->Attribute("id");
        ref.id = val ? val : "-1";
        val = elem->Attribute("parentID");
        ref.parentID = val ? val : "-1";
        val = elem->Attribute("restricted");
        if (val && strncmp(val, "true", 4) == 0)
          ref.restricted = true;
        else
          ref.restricted = false;
        ElementList vars;
        const tinyxml2::XMLElement* velem = elem->FirstChildElement();
        while (velem)
        {
          if (velem->Name() && velem->GetText())
          {
            ElementPtr var(new Element(DIDLDict.TranslateQName(xmlnames, velem->Name()), velem->GetText()));
            const tinyxml2::XMLAttribute* vattr = velem->FirstAttribute();
            while (vattr && vattr->Name() && vattr->Value())
            {
              var->SetAttribut(vattr->Name(), vattr->Value());
              vattr = vattr->Next();
            }
            vars.push_back(var);
          }
          velem = velem->NextSiblingElement();
        }
        m_items.push_back(DigitalItemPtr(new DigitalItem(ref.id, ref.parentID, ref.restricted, vars)));
      }
      elem = elem->NextSiblingElement();
    }
    return true;
  }
  return false;
}
