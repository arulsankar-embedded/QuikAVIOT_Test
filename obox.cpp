//-----------------------------------------------------------------------------
//
//	obox.cpp
//
//-----------------------------------------------------------------------------

#define _WINSOCK_DEPRECATED_NO_WARNINGS
//#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h> // socket related definitions
#include <winsock.h>
#include <ws2tcpip.h> // socket related definitions
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <WS2tcpip.h>

//#include "unistd.h"
#include "Options.h"
#include "Manager.h"
#include "Node.h"
#include "Group.h"
#include "Notification.h"
#include "Log.h"

#include "microhttpd.h"
#include "ozwcp.h"
#include "webserver.h"
//#include "XGetopt.h"
#include "oboxhttp.h"

using namespace OpenZWave;
using namespace std;


//#ifdef _MSC_VER//#pragma comment(lib, "pthreadVC2.lib")
//#pragma comment(lib, "libmicrohttpd.lib")
//#endif

static Webserver *wserver;
pthread_mutex_t nlock = PTHREAD_MUTEX_INITIALIZER;
MyNode *nodes[MAX_NODES];
int32 MyNode::nodecount = 0;
pthread_mutex_t glock = PTHREAD_MUTEX_INITIALIZER;
bool done = false;
bool needsave = false;
bool noop = false;
uint32 homeId = 0;
uint8 nodeId = 0;
uint8 SUCnodeId = 0;
const char *cmode = "";
int32 debug = false;
bool MyNode::nodechanged = false;
list<uint8> MyNode::removed;
uint8 Event_ZWave;
uint8 Err;
uint32 oboxhttp_count;	
char * IP;
char *path;

char config_path[1024];
char log_path[1024];
char command_path[1024];
/*
 * MyNode::MyNode constructor
 * Just save the nodes into an array and other initialization.
 */
MyNode::MyNode (int32 const ind) : type(0)
{
	if (ind < 1 || ind >= MAX_NODES) {
		Log::Write(LogLevel_Info, "new: bad node value %d, ignoring...", ind);
		delete this;
		return;
	}
	newGroup(ind);
	setTime(time(NULL));
	setChanged(true);
	nodes[ind] = this;
	nodecount++;
}

/*
 * MyNode::~MyNode destructor
 * Remove stored data.
 */
MyNode::~MyNode ()
{
	while (!values.empty()) {
		MyValue *v = values.back();
		values.pop_back();
		delete v;
	}
	while (!groups.empty()) {
		MyGroup *g = groups.back();
		groups.pop_back();
		delete g;
	}
}

/*
 * MyNode::remove
 * Remove node from array.
 */
void MyNode::remove (int32 const ind)
{
	if (ind < 1 || ind >= MAX_NODES) {
		Log::Write(LogLevel_Info, "remove: bad node value %d, ignoring...", ind);
		return;
	}
	if (nodes[ind] != NULL) {
		addRemoved(ind);
		delete nodes[ind];
		nodes[ind] = NULL;
		nodecount--;
	}
}

/*
 * compareValue
 * Function to compare values in the vector for sorting.
 */
bool compareValue (MyValue *a, MyValue *b)
{
	return (a->getId() < b->getId());
}

/*
 * MyNode::sortValues
 * Sort the ValueIDs
 */
void MyNode::sortValues ()
{
	sort(values.begin(), values.end(), compareValue);
	setChanged(true);
}
/*
 * MyNode::addValue
 * Per notifications, add a value to a node.
 */
void MyNode::addValue (ValueID id)
{
	MyValue *v = new MyValue(id);
	values.push_back(v);
	setTime(time(NULL));
	setChanged(true);
}

/*
 * MyNode::removeValue
 * Per notification, remove value from node.
 */
void MyNode::removeValue (ValueID id)
{
	vector<MyValue*>::iterator it;
	bool found = false;
	for (it = values.begin(); it != values.end(); it++) {
		if ((*it)->id == id) {
			delete *it;
			values.erase(it);
			found = true;
			break;
		}
	}
	if (!found)
		fprintf(stderr, "removeValue not found Home 0x%08x Node %d Genre %s Class %s Instance %d Index %d Type %s\n",
				id.GetHomeId(), id.GetNodeId(), valueGenreStr(id.GetGenre()),
				cclassStr(id.GetCommandClassId()), id.GetInstance(), id.GetIndex(),
				valueTypeStr(id.GetType()));

	setTime(time(NULL));
	setChanged(true);
}

/*
 * MyNode::saveValue
 * Per notification, update value info. Nothing really but update
 * tracking state.
 */
void MyNode::saveValue (ValueID id)
{
	setTime(time(NULL));
	setChanged(true);
}

/*
 * MyNode::newGroup
 * Get initial group information about a node.
 */
void MyNode::newGroup (uint8 node)
{
	int n = Manager::Get()->GetNumGroups(homeId, node);
	for (int i = 1; i <= n; i++) {
		MyGroup *p = new MyGroup();
		p->groupid = i;
		p->max = Manager::Get()->GetMaxAssociations(homeId, node, i);
		p->label = Manager::Get()->GetGroupLabel(homeId, node, i);
		groups.push_back(p);
	}
}

/*
 * MyNode::addGroup
 * Add group membership based on notification updates.
 */
void MyNode::addGroup (uint8 node, uint8 g, uint8 n, InstanceAssociation *v)
{
	fprintf(stderr, "addGroup: node %d group %d n %d\n", node, g, n);
	if (groups.size() == 0)
		newGroup(node);
	for (vector<MyGroup*>::iterator it = groups.begin(); it != groups.end(); ++it)
		if ((*it)->groupid == g) {
			(*it)->grouplist.clear();
			for (int i = 0; i < n; i++) {
				char str[32];
				if (v[i].m_instance == 0)
					snprintf( str, 32, "%d", v[i].m_nodeId );
				else
					snprintf( str, 32, "%d.%d", v[i].m_nodeId, v[i].m_instance );
				(*it)->grouplist.push_back(str);
			}
			setTime(time(NULL));
			setChanged(true);
			return;
		}
	fprintf(stderr, "addgroup: node %d group %d not found in list\n", node, g);
}

/*
 * MyNode::getGroup
 * Return group ptr for XML output
 */
MyGroup *MyNode::getGroup (uint8 i)
{
	for (vector<MyGroup*>::iterator it = groups.begin(); it != groups.end(); ++it)
		if ((*it)->groupid == i)
			return *it;
	return NULL;
}

/*
 * MyNode::updateGroup
 * Synchronize changes from user and update to network
 */
void MyNode::updateGroup (uint8 node, uint8 grp, char *glist)
{
	char *p = glist;
	vector<MyGroup*>::iterator it;
	char *np;
	vector<string> v;
	uint8 n;
	uint8 j;

	fprintf(stderr, "updateGroup: node %d group %d\n", node, grp);
	for (it = groups.begin(); it != groups.end(); ++it)
		if ((*it)->groupid == grp)
			break;
	if (it == groups.end()) {
		fprintf(stderr, "updateGroup: node %d group %d not found\n", node, grp);
		return;
	}
	n = 0;
	while (p != NULL && *p && n < (*it)->max) {
		np = strtok(p, ",");
		v.push_back( np );
		n++;
	}
	/* Look for nodes in the passed-in argument list, if not present add them */
	vector<string>::iterator nit;
	for (j = 0; j < n; j++) {
		for (nit = (*it)->grouplist.begin(); nit != (*it)->grouplist.end(); ++nit)
			if (v[j].compare( *nit ) == 0 )
				break;
		if (nit == (*it)->grouplist.end()) { // not found
			int nodeId = 0,  instance = 0;
			sscanf(v[j].c_str(),"%d.%d", &nodeId, &instance);
			Manager::Get()->AddAssociation(homeId, node, grp, nodeId, instance);
		}
	}
	/* Look for nodes in the vector (current list) and those not found in
     the passed-in list need to be removed */
	for (nit = (*it)->grouplist.begin(); nit != (*it)->grouplist.end(); ++nit) {
		for (j = 0; j < n; j++)
			if (v[j].compare( *nit ) == 0 )
				break;
		if (j >= n) {
			int nodeId = 0,  instance = 0;
			sscanf(nit->c_str(),"%d.%d", &nodeId, &instance);
			Manager::Get()->RemoveAssociation(homeId, node, grp, nodeId, instance);
		}
	}
}

/*
 * Scan list of values to be added to/removed from poll list
 */
void MyNode::updatePoll(char *ilist, char *plist)
{
	vector<char*> ids;
	vector<bool> polls;
	MyValue *v;
	char *p;
	char *np;

	p = ilist;
	while (p != NULL && *p) {
		np = strtok(p, ",");
		ids.push_back(np);
	}
	p = plist;
	while (p != NULL && *p) {
		np = strtok(p, ",");
		polls.push_back(*np == '1' ? true : false);
	}
	if (ids.size() != polls.size()) {
		fprintf(stderr, "updatePoll: size of ids %d not same as size of polls %d\n",
				ids.size(), polls.size());
		return;
	}
	vector<char*>::iterator it = ids.begin();
	vector<bool>::iterator pit = polls.begin();
	while (it != ids.end() && pit != polls.end()) {
		v = lookup(*it);
		if (v == NULL) {
			fprintf(stderr, "updatePoll: value %s not found\n", *it);
			continue;
		}
		/* if poll requested, see if not on list */
		if (*pit) {
			if (!Manager::Get()->isPolled(v->getId()))
				if (!Manager::Get()->EnablePoll(v->getId()))
					fprintf(stderr, "updatePoll: enable polling for %s failed\n", *it);
		} else {			// polling not requested and it is on, turn it off
			if (Manager::Get()->isPolled(v->getId()))
				if (!Manager::Get()->DisablePoll(v->getId()))
					fprintf(stderr, "updatePoll: disable polling for %s failed\n", *it);
		}
		++it;
		++pit;
	}
}

/*
 * Parse textualized value representation in the form of:
 * 2-SWITCH MULTILEVEL-user-byte-1-0
 * node-class-genre-type-instance-index
 */
MyValue *MyNode::lookup (string data)
{
	uint8 node = 0;
	uint8 cls;
	uint8 inst;
	uint8 ind;
	ValueID::ValueGenre vg;
	ValueID::ValueType typ;
	size_t pos1, pos2;
	string str;

	node = strtol(data.c_str(), NULL, 10);
	if (node == 0)
		return NULL;
	pos1 = data.find("-", 0);
	if (pos1 == string::npos)
		return NULL;
	pos2 = data.find("-", ++pos1);
	if (pos2 == string::npos)
		return NULL;
	str = data.substr(pos1, pos2 - pos1);
	cls = cclassNum(str.c_str());
	if (cls == 0xFF)
		return NULL;
	pos1 = pos2;
	pos2 = data.find("-", ++pos1);
	if (pos2 == string::npos)
		return NULL;
	str = data.substr(pos1, pos2 - pos1);
	vg = valueGenreNum(str.c_str());
	pos1 = pos2;
	pos2 = data.find("-", ++pos1);
	if (pos2 == string::npos)
		return NULL;
	str = data.substr(pos1, pos2 - pos1);
	typ = valueTypeNum(str.c_str());
	pos1 = pos2;
	pos2 = data.find("-", ++pos1);
	if (pos2 == string::npos)
		return NULL;
	str = data.substr(pos1, pos2 - pos1);
	inst = strtol(str.c_str(), NULL, 10);
	pos1 = pos2 + 1;
	str = data.substr(pos1);
	ind = strtol(str.c_str(), NULL, 10);
	ValueID id(homeId, node, vg, cls, inst, ind, typ);
	MyNode *n = nodes[node];
	if (n == NULL)
		return NULL;
	for (vector<MyValue*>::iterator it = n->values.begin(); it != n->values.end(); it++)
		if ((*it)->id == id)
			return *it;
	return NULL;
}

/*
 * Returns a count of values
 */
uint16 MyNode::getValueCount ()
{
	return values.size();
}

/*
 * Returns an n'th value
 */
MyValue *MyNode::getValue (uint16 n)
{
	if (n < values.size())
		return values[n];
	return NULL;
}

/*
 * Mark all nodes as changed
 */
void MyNode::setAllChanged (bool ch)
{
	nodechanged = ch;
	int i = 0;
	int j = 1;
	while (j <= nodecount && i < MAX_NODES) {
		if (nodes[i] != NULL) {
			nodes[i]->setChanged(true);
			j++;
		}
		i++;
	}
}

/*
 * Returns next i
 on the removed list.
 */

uint8 MyNode::getRemoved()
{
	if (removed.size() > 0) {
		uint8 node = removed.front();
		removed.pop_front();
		return node;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// <OnNotification>
// Callback that is triggered when a value, group or node changes
//----------------------------------;'-------------------------------------------
void OnNotification (Notification const* _notification, void* _context)
{
	ValueID id = _notification->GetValueID();
	switch (_notification->GetType()) {
		case Notification::Type_ValueAdded:
			Log::Write(LogLevel_Info, "Notification: Value Added Home 0x%08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->addValue(id);
			nodes[_notification->GetNodeId()]->setTime(time(NULL));
			nodes[_notification->GetNodeId()]->setChanged(true);
			pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_ValueRemoved:
			Log::Write(LogLevel_Info, "Notification: Value Removed Home 0x%08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->removeValue(id);
			nodes[_notification->GetNodeId()]->setTime(time(NULL));
			nodes[_notification->GetNodeId()]->setChanged(true);
			pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_ValueChanged:
			Log::Write(LogLevel_Info, "Notification: Value Changed Home 0x%08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->saveValue(id);
			pthread_mutex_unlock(&nlock);
			OBoxlog();
			break;
		case Notification::Type_ValueRefreshed:
			Log::Write(LogLevel_Info, "Notification: Value Refreshed Home 0x%08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->setTime(time(NULL));
			nodes[_notification->GetNodeId()]->setChanged(true);
			pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_Group:
		{
			Log::Write(LogLevel_Info, "Notification: Group Home 0x%08x Node %d Group %d",
					_notification->GetHomeId(), _notification->GetNodeId(), _notification->GetGroupIdx());
			InstanceAssociation *v = NULL;
			int8 n = Manager::Get()->GetAssociations(homeId, _notification->GetNodeId(), _notification->GetGroupIdx(), &v);
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->addGroup(_notification->GetNodeId(), _notification->GetGroupIdx(), n, v);
			pthread_mutex_unlock(&nlock);
			if (v != NULL)
				delete [] v;
		}
		break;
		case Notification::Type_NodeNew:
			Log::Write(LogLevel_Info, "Notification: Node New Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&glock);
			needsave = true;
			pthread_mutex_unlock(&glock);
			break;
		case Notification::Type_NodeAdded:
			Log::Write(LogLevel_Info, "Notification: Node Added Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			new MyNode(_notification->GetNodeId());
			pthread_mutex_unlock(&nlock);
			pthread_mutex_lock(&glock);
			needsave = true;
			pthread_mutex_unlock(&glock);
			OBoxlog();
			break;
		case Notification::Type_NodeRemoved:
			Log::Write(LogLevel_Info, "Notification: Node Removed Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			MyNode::remove(_notification->GetNodeId());
			pthread_mutex_unlock(&nlock);
			pthread_mutex_lock(&glock);
			needsave = true;
			pthread_mutex_unlock(&glock);
			OBoxlog();
			break;
		case Notification::Type_NodeProtocolInfo:
			Log::Write(LogLevel_Info, "Notification: Node Protocol Info Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->saveValue(id);
			pthread_mutex_unlock(&nlock);
			pthread_mutex_lock(&glock);
			needsave = true;
			pthread_mutex_unlock(&glock);
			break;
		case Notification::Type_NodeNaming:
			Log::Write(LogLevel_Info, "Notification: Node Naming Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->saveValue(id);
			pthread_mutex_unlock(&nlock);
			OBoxlog();
			break;
		case Notification::Type_NodeEvent:
			Log::Write(LogLevel_Info, "Notification: Node Event Home %08x Node %d Status %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(), _notification->GetEvent(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->saveValue(id);
			pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_PollingDisabled:
			Log::Write(LogLevel_Info, "Notification: Polling Disabled Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			//pthread_mutex_lock(&nlock);
			//nodes[_notification->GetNodeId()]->setPolled(false);
			//pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_PollingEnabled:
			Log::Write(LogLevel_Info, "Notification: Polling Enabled Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()));
			//pthread_mutex_lock(&nlock);
			//nodes[_notification->GetNodeId()]->setPolled(true);
			//pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_SceneEvent:
			Log::Write(LogLevel_Info, "Notification: Scene Event Home %08x Node %d Genre %s Class %s Instance %d Index %d Type %s Scene Id %d",
					_notification->GetHomeId(), _notification->GetNodeId(),
					valueGenreStr(id.GetGenre()), cclassStr(id.GetCommandClassId()), id.GetInstance(),
					id.GetIndex(), valueTypeStr(id.GetType()), _notification->GetSceneId());
			break;
		case Notification::Type_CreateButton:
			Log::Write(LogLevel_Info, "Notification: Create button Home %08x Node %d Button %d",
					_notification->GetHomeId(), _notification->GetNodeId(), _notification->GetButtonId());
			break;
		case Notification::Type_DeleteButton:
			Log::Write(LogLevel_Info, "Notification: Delete button Home %08x Node %d Button %d",
					_notification->GetHomeId(), _notification->GetNodeId(), _notification->GetButtonId());
			break;
		case Notification::Type_ButtonOn:
			Log::Write(LogLevel_Info, "Notification: Button On Home %08x Node %d Button %d",
					_notification->GetHomeId(), _notification->GetNodeId(), _notification->GetButtonId());
			break;
		case Notification::Type_ButtonOff:
			Log::Write(LogLevel_Info, "Notification: Button Off Home %08x Node %d Button %d",
					_notification->GetHomeId(), _notification->GetNodeId(), _notification->GetButtonId());
			break;
		case Notification::Type_DriverReady:
			Log::Write(LogLevel_Info, "Notification: Driver Ready, homeId %08x, nodeId %d", _notification->GetHomeId(),
					_notification->GetNodeId());
			pthread_mutex_lock(&glock);
			homeId = _notification->GetHomeId();
			nodeId = _notification->GetNodeId();
			if (Manager::Get()->IsStaticUpdateController(homeId)) {
				cmode = "SUC";
				SUCnodeId = Manager::Get()->GetSUCNodeId(homeId);
			} else if (Manager::Get()->IsPrimaryController(homeId))
				cmode = "Primary";
			else
				cmode = "Slave";
			pthread_mutex_unlock(&glock);
			break;
		case Notification::Type_DriverFailed:
			Log::Write(LogLevel_Info, "Notification: Driver Failed, homeId %08x", _notification->GetHomeId());
			pthread_mutex_lock(&glock);
			done = false;
			needsave = false;
			homeId = 0;
			cmode = "";
			pthread_mutex_unlock(&glock);
			pthread_mutex_lock(&nlock);
			for (int i = 1; i < MAX_NODES; i++)
				MyNode::remove(i);
			pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_DriverReset:
			Log::Write(LogLevel_Info, "Notification: Driver Reset, homeId %08x", _notification->GetHomeId());
			pthread_mutex_lock(&glock);
			done = false;
			needsave = true;
			homeId = _notification->GetHomeId();
			if (Manager::Get()->IsStaticUpdateController(homeId)) {
				cmode = "SUC";
				SUCnodeId = Manager::Get()->GetSUCNodeId(homeId);
			} else if (Manager::Get()->IsPrimaryController(homeId))
				cmode = "Primary";
			else
				cmode = "Slave";
			pthread_mutex_unlock(&glock);
			pthread_mutex_lock(&nlock);
			for (int i = 1; i < MAX_NODES; i++)
				MyNode::remove(i);
			pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_EssentialNodeQueriesComplete:
			Log::Write(LogLevel_Info, "Notification: Essential Node %d Queries Complete", _notification->GetNodeId());
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->setTime(time(NULL));
			nodes[_notification->GetNodeId()]->setChanged(true);
			pthread_mutex_unlock(&nlock);
			break;
		case Notification::Type_NodeQueriesComplete:
			Log::Write(LogLevel_Info, "Notification: Node %d Queries Complete", _notification->GetNodeId());
			pthread_mutex_lock(&nlock);
			nodes[_notification->GetNodeId()]->sortValues();
			nodes[_notification->GetNodeId()]->setTime(time(NULL));
			nodes[_notification->GetNodeId()]->setChanged(true);
			pthread_mutex_unlock(&nlock);
			pthread_mutex_lock(&glock);
			needsave = true;
			pthread_mutex_unlock(&glock);
			break;
		case Notification::Type_AwakeNodesQueried:
			Log::Write(LogLevel_Info, "Notification: Awake Nodes Queried");
			break;
		case Notification::Type_AllNodesQueriedSomeDead:
			Log::Write(LogLevel_Info, "Notification: Awake Nodes Queried Some Dead");
			break;
		case Notification::Type_AllNodesQueried:
			OBoxlog();
			Log::Write(LogLevel_Info, "Notification: All Nodes Queried");
			break;
		case Notification::Type_Notification:
			switch (_notification->GetNotification()) {
				case Notification::Code_MsgComplete:
					Log::Write(LogLevel_Info, "Notification: Notification home %08x node %d Message Complete",
							_notification->GetHomeId(), _notification->GetNodeId());
					break;
				case Notification::Code_Timeout:
					Log::Write(LogLevel_Info, "Notification: Notification home %08x node %d Timeout",
							_notification->GetHomeId(), _notification->GetNodeId());
					break;
				case Notification::Code_NoOperation:
					Log::Write(LogLevel_Info, "Notification: Notification home %08x node %d No Operation Message Complete",
							_notification->GetHomeId(), _notification->GetNodeId());
					pthread_mutex_lock(&glock);
					noop = true;
					pthread_mutex_unlock(&glock);
					break;
				case Notification::Code_Awake:
					Log::Write(LogLevel_Info, "Notification: Notification home %08x node %d Awake",
							_notification->GetHomeId(), _notification->GetNodeId());
					pthread_mutex_lock(&nlock);
					nodes[_notification->GetNodeId()]->setTime(time(NULL));
					nodes[_notification->GetNodeId()]->setChanged(true);
					pthread_mutex_unlock(&nlock);
					break;
				case Notification::Code_Sleep:
					Log::Write(LogLevel_Info, "Notification: Notification home %08x node %d Sleep",
							_notification->GetHomeId(), _notification->GetNodeId());
					pthread_mutex_lock(&nlock);
					nodes[_notification->GetNodeId()]->setTime(time(NULL));
					nodes[_notification->GetNodeId()]->setChanged(true);
					pthread_mutex_unlock(&nlock);
					break;
				case Notification::Code_Dead:
					Log::Write(LogLevel_Info, "Notification: Notification home %08x node %d Dead",
							_notification->GetHomeId(), _notification->GetNodeId());
					pthread_mutex_lock(&nlock);
					nodes[_notification->GetNodeId()]->setTime(time(NULL));
					nodes[_notification->GetNodeId()]->setChanged(true);
					pthread_mutex_unlock(&nlock);
					break;
				default:
					Log::Write(LogLevel_Info, "Notification: Notification home %08x node %d Unknown %d",
							_notification->GetHomeId(), _notification->GetNodeId(), _notification->GetNotification());
					break;
			}
			break;
			case Notification::Type_ControllerCommand:
				Log::Write(LogLevel_Info, "Notification: ControllerCommand home %08x Event %d Error %d",
						_notification->GetHomeId(), _notification->GetEvent(), _notification->GetNotification());
				pthread_mutex_lock(&nlock);
				Event_ZWave = _notification->GetEvent();
				Err = _notification->GetNotification();
				//fprintf(stderr, "CS Command:%d/n", Event_ZWave);
				web_controller_update((OpenZWave::Driver::ControllerState)_notification->GetEvent(), (OpenZWave::Driver::ControllerError)_notification->GetNotification(), (void *)_context);
				pthread_mutex_unlock(&nlock);
				break;
			default:
					Log::Write(LogLevel_Info, "Notification: type %d home %08x node %d genre %d class %d instance %d index %d type %d",
							_notification->GetType(), _notification->GetHomeId(),
							_notification->GetNodeId(), id.GetGenre(), id.GetCommandClassId(),
							id.GetInstance(), id.GetIndex(), id.GetType());
					break;
	}
}

/*OBOXHTTP GET/POST RESPONSE made by ArulSankar*/

void OnBegin(const oboxhttp::Response* r, void* userdata)
{
	printf("BEGIN (%d %s)\n", r->getstatus(), r->getreason());
	oboxhttp_count = 0;
}

void OnData(const oboxhttp::Response* r, void* userdata, const unsigned char* data, int n)
{
	fwrite(data, 1, n, stdout);
	oboxhttp_count += n;
}

void OnComplete(const oboxhttp::Response* r, void* userdata)
{
	printf("COMPLETE (%d bytes)\n", oboxhttp_count);
}

void OBoxPolling()
{
	//char *LocalHost = "localhost";
	oboxhttp::Connection conn("localhost", DEFAULT_PORT);
	
	conn.setcallbacks(OnBegin, OnData, OnComplete, 0);

	conn.request("GET", "/devicepollingevery3seconds", 0, 0, 0);
	while (conn.outstanding())
		conn.pump();
	fprintf(stderr, "IP of this scope is: %s\n", IP);
}
void OBoxlog()
{
	//char *LocalHost = "localhost";
	oboxhttp::Connection conn("localhost", DEFAULT_PORT);

	conn.setcallbacks(OnBegin, OnData, OnComplete, 0);

	conn.request("GET", "/user_request?action=devicedetails&deviceid=all&save=YES", 0, 0, 0);

	while (conn.outstanding())
	{
		conn.pump();
	}
	//fprintf(stderr, "IP of this scope is: %s\n", IP);
}

/*Get IP Address*/

int GetIpaddress()
{
	char ac[80];
	
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
		return 1;
	}
	std::cout << "Host name is " << ac << "." << std::endl;

	struct hostent *phe = gethostbyname(ac);
	if (phe == 0) {
		return 1;
	}

	for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		IP = inet_ntoa(addr);	
		std::cout << "Address " << i << ": " << IP << std::endl;
	}

	return 0;
}

void *oboxpollingresponse(void*)
{
	while (1)
	{
		//printf("I am a thread !!!!!!!!!!!!!!\n");
		OBoxPolling();
		Sleep(3);
	}
}

int GetConfig_Log_Path(char word[])
{
	FILE *f;
	char buf[1024];
	int ch = 0;
	int i = 0;

	f = fopen("C:\\Program Files\\QuikAV\\IOT\\config.txt", "r");
	if (f == NULL)
		return 1;
	while (fgets(buf, 1024, f) != NULL) {
		if (strstr(buf, word) != NULL)
		{

			i = strlen(word);
			if ((strcmp("config_path:", word)) == 0)
			{
				while (buf[i] != '\n')
				{
					config_path[ch] = buf[i];
					ch++;
					i++;
				}
				ch++;
				config_path[ch] = '\n';
				printf("ConfigPathis=%s\n", config_path);
			}
			else if ((strcmp("logs_path:", word)) == 0)
			{
				while (buf[i] != '\n')
				{
					log_path[ch] = buf[i];
					ch++;
					i++;
				}
				ch++;
				log_path[ch] = '\n';
				printf("LogPathis=%s\n", log_path);
			}
			else if ((strcmp("command_path:", word)) == 0)
			{
				while (buf[i] != '\n')
				{
					command_path[ch] = buf[i];
					ch++;
					i++;
				}
				ch++;
				command_path[ch] = '\n';
				printf("CommandPathis=%s\n", command_path);
			}
			else
			{
				break;
			}
		}
		ch = 0;
	}
	fclose(f);
	return 0;
}
/*
char * GetConfigPath()
{
	FILE *fpath = NULL;
	char config_path[512];
	char *temp;

	fpath = fopen("C:\\ProgramData\\QuikAV\\IOT\\config.txt", "r+");
	if (fpath == NULL)
	{
		printf("C:\\ProgramData\\QuikAV\\IOT\\config.txt doesn't exit\n");
	}
	while (fgets(config_path, 512, fpath) != NULL) {
		if ((temp = strchr(config_path, ':')) != NULL) {
			printf("path : %s\n", temp);
		}
	}
	path = temp;
	fclose(fpath);
	return path;
}
*/
//-----------------------------------------------------------------------------
// <main>
// Create the driver and then wait
//-----------------------------------------------------------------------------
int32 main(int32 argc, char* argv[])
{
	int32 i;
	
	char *optarg;
	long webport = DEFAULT_PORT;
	char *ptr;
	pthread_t OBoxPolling_thread;
	GetIpaddress();
	char *c_path;
	if (argc != 2)
	{
		fprintf(stderr, "usage: obox <serial port>\n");
	}

#if 0
	while ((i = getopt(argc, argv, "dp:")) != EOF)
		switch (i) {
			case 'd':
				debug = 1;
				break;
			case 'p':

				webport = strtol(optarg, &ptr, 10);
				if (ptr == optarg)
					goto bad;
				break;
			default:
				bad:
				fprintf(stderr, "usage: ozwcp [-d] -p <port>\n");
			exit(1);
		}
#endif
	for (i = 0; i < MAX_NODES; i++)
		nodes[i] = NULL;

	//c_path = GetConfigPath();
	GetConfig_Log_Path("logs_path:");
	GetConfig_Log_Path("config_path:");
	GetConfig_Log_Path("command_path:");

	//printf("c_path:%s\n", c_path);

	Options::Create(config_path, command_path, "--SaveConfiguration=true --DumpTriggerLevel=0");
	Options::Get()->Lock();

	Manager::Create();
	wserver = new Webserver(webport, argv[1]);
	Manager::Get()->AddWatcher(OnNotification, wserver);

	while (!wserver->isReady()) {
		delete wserver;
		Sleep(2000);
		wserver = new Webserver(webport, argv[1]);
	}
	pthread_create(&OBoxPolling_thread, NULL, &oboxpollingresponse, NULL);
	pthread_join(OBoxPolling_thread, NULL);
	while (!done) {	// now wait until we are done
		Sleep(1000);
	}

	delete wserver;
	Manager::Get()->RemoveWatcher(OnNotification, NULL);
	Manager::Destroy();
	Options::Destroy();
	
	exit(0);
}
