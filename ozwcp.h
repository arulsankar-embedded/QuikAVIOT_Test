//-----------------------------------------------------------------------------
//
//	ozwcp.h
//
//	OpenZWave Control Panel
//
//	Copyright (c) 2010 Greg Satz <satz@iranger.com>
//	All rights reserved.
//
// SOFTWARE NOTICE AND LICENSE
// This work (including software, documents, or other related items) is being 
// provided by the copyright holders under the following license. By obtaining,
// using and/or copying this work, you (the licensee) agree that you have read,
// understood, and will comply with the following terms and conditions:
//
// Permission to use, copy, and distribute this software and its documentation,
// without modification, for any purpose and without fee or royalty is hereby 
// granted, provided that you include the full text of this NOTICE on ALL
// copies of the software and documentation or portions thereof.
//
// THIS SOFTWARE AND DOCUMENTATION IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS 
// MAKE NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR 
// PURPOSE OR THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE 
// ANY THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
//
// COPYRIGHT HOLDERS WILL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL OR 
// CONSEQUENTIAL DAMAGES ARISING OUT OF ANY USE OF THE SOFTWARE OR 
// DOCUMENTATION.
//
// The name and trademarks of copyright holders may NOT be used in advertising 
// or publicity pertaining to the software without specific, written prior 
// permission.  Title to copyright in this software and any associated 
// documentation will at all times remain with copyright holders.
//-----------------------------------------------------------------------------

#include <list>
#include <algorithm>
#include "Driver.h"
#include "Notification.h"
#include "ValueStore.h"
#include "Value.h"
#include "ValueBool.h"
#include "ValueByte.h"
#include "ValueDecimal.h"
#include "ValueInt.h"
#include "ValueList.h"
#include "ValueShort.h"
#include "ValueString.h"

using namespace OpenZWave;

#define MAX_NODES 255
#define DEFAULT_PORT 8090
//extern const char *cmode = "";

extern const char *valueGenreStr(ValueID::ValueGenre);
extern ValueID::ValueGenre valueGenreNum(char const *);
extern const char *valueTypeStr(ValueID::ValueType);
extern ValueID::ValueType valueTypeNum(char const *);
extern const char *nodeBasicStr(uint8);
extern const char *cclassStr(uint8);
extern uint8 cclassNum(char const *str);
extern const char *controllerErrorStr(Driver::ControllerError err);
void OBoxlog();



class MyValue {
  friend class MyNode;

 public:
 MyValue(ValueID id) : id(id) {};
  ~MyValue() {};
  ValueID getId() { return id; }
 private:  
  ValueID id;
};

typedef struct {
  uint8 groupid;
  uint8 max;
  string label;
  vector<string> grouplist;
} MyGroup;

class MyNode {
public:
  MyNode(int32 const);
  static void remove(int32 const);
  static int32 getNodeCount() { return nodecount; };      
  static int32 resetnodecount() { nodecount = 1; return nodecount; };
  void sortValues();
  void addValue(ValueID id);
  void removeValue(ValueID id);
  void saveValue(ValueID id);
  uint16 getValueCount();
  static MyValue *lookup(string id);
  MyValue *getValue(uint16 n);
  uint32 getTime() { return mtime; }
  void setTime(uint32 t) { mtime = t; }
  static bool getAnyChanged() { return nodechanged; }
  static void setAllChanged(bool ch);
  bool getChanged() { return changed; }
  void setChanged(bool ch) { changed = ch; nodechanged = ch; }
  static void addRemoved(uint8 node) { removed.push_back(node); }
  static uint32 getRemovedCount() { return removed.size(); }
  static uint8 getRemoved();
  void addGroup(uint8 node, uint8 g, uint8 n, InstanceAssociation *v);
  MyGroup *getGroup(uint8 i);
  void updateGroup(uint8 node, uint8 grp, char *glist);
  uint8 numGroups() { return (unsigned int)groups.size(); }
  void updatePoll(char *ilist, char *plist);
private:
  ~MyNode();
  void newGroup(uint8 n);
  static int32 nodecount;
  int32 type;
  uint32 mtime;
  bool changed;
  static bool nodechanged;
  static list<uint8> removed;
  vector<MyGroup*> groups;
  vector<MyValue*> values;
};
