//========================================================================
//
// OptionalContent.h
//
// Copyright 2007 Brad Hards <bradh@kde.org>
//
// Released under the GPL (version 2, or later, at your option)
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "goo/gmem.h"
#include "goo/GooString.h"
#include "goo/GooList.h"
#include "Error.h"
// #include "PDFDocEncoding.h"
#include "OptionalContent.h"

//------------------------------------------------------------------------

OCGs::OCGs(Object *ocgObject, XRef *xref) :
  m_orderArray(0), m_rBGroupsArray(), m_xref(xref)
{
  optionalContentGroups = NULL;

  if (!ocgObject->isDict()) {
    // This isn't an error - OCProperties is optional.
    return;
  }

  // we need to parse the dictionary here, and build optionalContentGroups
  optionalContentGroups = new GooList();

  Object ocgList;
  ocgObject->dictLookup("OCGs", &ocgList);
  if (!ocgList.isArray()) {
    error(-1, "Expected the optional content group list, but wasn't able to find it, or it isn't an Array");
    ocgList.free();
    return;
  }

  // we now enumerate over the ocgList, and build up the optionalContentGroups list.
  for(int i = 0; i < ocgList.arrayGetLength(); ++i) {
    Object ocg;
    ocgList.arrayGet(i, &ocg);
    if (!ocg.isDict()) {
      ocg.free();
      break;
    }
    OptionalContentGroup *thisOptionalContentGroup = new OptionalContentGroup(ocg.getDict(), xref);
    ocg.free();
    ocgList.arrayGetNF(i, &ocg);
    // TODO: we should create a lookup map from Ref to the OptionalContentGroup
    thisOptionalContentGroup->setRef( ocg.getRef() );
    ocg.free();
    // the default is ON - we change state later, depending on BaseState, ON and OFF
    thisOptionalContentGroup->setState(OptionalContentGroup::On);
    optionalContentGroups->append(thisOptionalContentGroup);
  }

  Object defaultOcgConfig;
  ocgObject->dictLookup("D", &defaultOcgConfig);
  if (!defaultOcgConfig.isDict()) {
    error(-1, "Expected the default config, but wasn't able to find it, or it isn't a Dictionary");
    defaultOcgConfig.free();
    ocgList.free();
    return;
  }
#if 0
  // this is untested - we need an example showing BaseState
  Object baseState;
  defaultOcgConfig.dictLookup("BaseState", &baseState);
  if (baseState.isString()) {
    // read the value, and set each OptionalContentGroup entry appropriately
  }
  baseState.free();
#endif
  Object on;
  defaultOcgConfig.dictLookup("ON", &on);
  if (on.isArray()) {
    // ON is an optional element
    for (int i = 0; i < on.arrayGetLength(); ++i) {
      Object reference;
      on.arrayGetNF(i, &reference);
      if (!reference.isRef()) {
	// there can be null entries
	reference.free();
	break;
      }
      OptionalContentGroup *group = findOcgByRef( reference.getRef() );
      reference.free();
      if (!group) {
	error(-1, "Couldn't find group for reference");
	break;
      }
      group->setState(OptionalContentGroup::On);
    }
  }
  on.free();

  Object off;
  defaultOcgConfig.dictLookup("OFF", &off);
  if (off.isArray()) {
    // OFF is an optional element
    for (int i = 0; i < off.arrayGetLength(); ++i) {
      Object reference;
      off.arrayGetNF(i, &reference);
      if (!reference.isRef()) {
	// there can be null entries
	reference.free();
	break;
      }
      OptionalContentGroup *group = findOcgByRef( reference.getRef() );
      reference.free();
      if (!group) {
	error(-1, "Couldn't find group for reference to set OFF");
	break;
      }
      group->setState(OptionalContentGroup::Off);
    }
  }
  off.free();

  Object order;
  defaultOcgConfig.dictLookup("Order", &order);
  if ( (order.isArray()) && (order.arrayGetLength() > 0) ) {
    m_orderArray = order.getArray();
  }

  Object rbgroups;
  defaultOcgConfig.dictLookup("RBGroups", &rbgroups);
  if ( (rbgroups.isArray()) && (rbgroups.arrayGetLength() > 0) ) {
    m_rBGroupsArray = rbgroups.getArray();
  }

  ocgList.free();
  defaultOcgConfig.free();
}

OCGs::~OCGs()
{
  if (optionalContentGroups) {
    deleteGooList(optionalContentGroups, OptionalContentGroup);
  }
}


bool OCGs::hasOCGs()
{
  if (!optionalContentGroups) {
    return false;
  }
  return ( optionalContentGroups->getLength() > 0 );
}

OptionalContentGroup* OCGs::findOcgByRef( const Ref &ref)
{
  //TODO: make this more efficient
  OptionalContentGroup *ocg = NULL;
  if (optionalContentGroups != NULL)
  {
    for (int i=0; i < optionalContentGroups->getLength(); ++i) {
      ocg = (OptionalContentGroup*)optionalContentGroups->get(i);
      if ( (ocg->ref().num == ref.num) && (ocg->ref().gen == ref.gen) ) {
        return ocg;
      }
    }
  }

  error(-1, "Could not find a OCG with Ref (%d:%d)", ref.num, ref.gen);

  // not found
  return NULL;
}

bool OCGs::optContentIsVisible( Object *dictRef )
{
  Object dictObj;
  Dict *dict;
  Object dictType;
  Object ocg;
  Object policy;
  bool result = true;
  dictRef->fetch( m_xref, &dictObj );
  if ( ! dictObj.isDict() ) {
    error(-1, "Unexpected oc reference target: %i", dictObj.getType() );
    dictObj.free();
    return result;
  }
  dict = dictObj.getDict();
  // printf("checking if optContent is visible\n");
  dict->lookup("Type", &dictType);
  if (dictType.isName("OCMD")) {
    // If we supported Visibility Expressions, we'd check
    // for a VE entry, and then call out to the parser here...
    // printf("found OCMD dict\n");
    dict->lookup("P", &policy);
    dict->lookupNF("OCGs", &ocg);
    if (ocg.isArray()) {
      if (policy.isName("AllOn")) {
	result = allOn( ocg.getArray() );
      } else if (policy.isName("AllOff")) {
	result = allOff( ocg.getArray() );
      } else if (policy.isName("AnyOff")) {
	result = anyOff( ocg.getArray() );
      } else if ( (!policy.isName()) || (policy.isName("AnyOn") ) ) {
	// this is the default
	result = anyOn( ocg.getArray() );
      }
    } else if (ocg.isRef()) {
      OptionalContentGroup* oc = findOcgByRef( ocg.getRef() );      
      if ( !oc || oc->state() == OptionalContentGroup::Off ) {
	result = false;
      } else {
	result = true ;
      }
    }
    ocg.free();
    policy.free();
  } else if ( dictType.isName("OCG") ) {
    OptionalContentGroup* oc = findOcgByRef( dictRef->getRef() );
    if ( !oc || oc->state() == OptionalContentGroup::Off ) {
      result=false;
    }
  }
  dictType.free();
  dictObj.free();
  // printf("visibility: %s\n", result? "on" : "off");
  return result;
}

bool OCGs::allOn( Array *ocgArray )
{
  for (int i = 0; i < ocgArray->getLength(); ++i) {
    Object ocgItem;
    ocgArray->getNF(i, &ocgItem);
    if (ocgItem.isRef()) {
      OptionalContentGroup* oc = findOcgByRef( ocgItem.getRef() );      
      if ( oc && oc->state() == OptionalContentGroup::Off ) {
	return false;
      }
    }
  }
  return true;
}

bool OCGs::allOff( Array *ocgArray )
{
  for (int i = 0; i < ocgArray->getLength(); ++i) {
    Object ocgItem;
    ocgArray->getNF(i, &ocgItem);
    if (ocgItem.isRef()) {
      OptionalContentGroup* oc = findOcgByRef( ocgItem.getRef() );      
      if ( oc && oc->state() == OptionalContentGroup::On ) {
	return false;
      }
    }
  }
  return true;
}

bool OCGs::anyOn( Array *ocgArray )
{
  for (int i = 0; i < ocgArray->getLength(); ++i) {
    Object ocgItem;
    ocgArray->getNF(i, &ocgItem);
    if (ocgItem.isRef()) {
      OptionalContentGroup* oc = findOcgByRef( ocgItem.getRef() );      
      if ( oc && oc->state() == OptionalContentGroup::On ) {
	return true;
      }
    }
  }
  return false;
}

bool OCGs::anyOff( Array *ocgArray )
{
  for (int i = 0; i < ocgArray->getLength(); ++i) {
    Object ocgItem;
    ocgArray->getNF(i, &ocgItem);
    if (ocgItem.isRef()) {
      OptionalContentGroup* oc = findOcgByRef( ocgItem.getRef() );      
      if ( oc && oc->state() == OptionalContentGroup::Off ) {
	return true;
      }
    }
  }
  return false;
}

//------------------------------------------------------------------------

OptionalContentGroup::OptionalContentGroup(Dict *ocgDict, XRef *xrefA)
{
  Object ocgName;
  ocgDict->lookupNF("Name", &ocgName);
  if (!ocgName.isString()) {
    error(-1, "Expected the name of the OCG, but wasn't able to find it, or it isn't a String");
  } else {
    m_name = new GooString( ocgName.getString() );
  }
  ocgName.free();
}

OptionalContentGroup::OptionalContentGroup(GooString *label)
{
  m_name = label;
  m_state = On;
}

GooString* OptionalContentGroup::name() const
{
  return m_name;
}

void OptionalContentGroup::setRef(const Ref ref)
{
  m_ref = ref;
}

Ref OptionalContentGroup::ref() const
{
  return m_ref;
}

OptionalContentGroup::~OptionalContentGroup()
{
  delete m_name;
}

