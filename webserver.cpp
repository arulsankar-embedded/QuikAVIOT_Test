//
//	webserver.cpp -- libmicrohttpd web interface for Obox
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_WARNINGS 
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <sys/stat.h>
#include <winsock2.h> // socket related definitions
#include <ws2tcpip.h> // socket related definitions
#include <sys/types.h>
#include <pthread.h>
#include <iostream>
#include <fstream>


#include "Manager.h"
#include "Driver.h"
#include "Node.h"
#include "ValueBool.h"
#include "ValueByte.h"
#include "ValueDecimal.h"
#include "ValueInt.h"
#include "ValueList.h"
#include "ValueRaw.h"
#include "ValueShort.h"
#include "ValueString.h"
#include "tinyxml.h"
//#include "unistd.h"

#include "microhttpd.h"
#include "ozwcp.h"
#include "webserver.h"
#include "json.h"
#if defined(_WIN32)
#define strdup _strdup
#endif

using namespace OpenZWave;

#define NBSP(str)	(str) == NULL || (*str) == '\0' ? "&nbsp;" : (str)
#define BLANK(str)	(str) == NULL || (*str) == '\0' ? "" : (str)
#define FNF "<html><head><title>File not found</title></head><body>File not found: %s</body></html>"
#define UNKNOWN "<html><head><title>Nothingness</title></head><body>There is nothing here. Sorry.</body></html>\n"
#define DEFAULT "<script type=\"text/javascript\"> document.location.href='/';</script>"
#define EMPTY "<html></html>"

typedef struct _conninfo {
		conntype_t conn_type;
		char const *conn_url;
		void *conn_arg1;
		void *conn_arg2;
		void *conn_arg3;
		void *conn_arg4;
		void *conn_res;
		struct MHD_PostProcessor *conn_pp;
} conninfo_t;

bool Webserver::usb = false;
char *Webserver::devname = NULL;
unsigned short Webserver::port = 0;
bool Webserver::ready = false;

extern pthread_mutex_t nlock;
extern MyNode *nodes[];
Notification const* notification;
extern pthread_mutex_t glock;
extern bool done;
extern bool needsave;
extern uint32 homeId;		// controller home id
extern uint8 nodeId;	   // controller node id
extern uint8 SUCnodeId;
extern uint8 Event_ZWave;
extern char *IP;
char *cmode;
extern bool noop;
extern int debug;
struct tm tm;
char dev_id[9];
//static char buffer[] = "wget -O test.zip https://obox-1271.appspot.com/rest/obox/download";
static char Fversion[5];
static int update_status = 0;
string command_str;
string msg;
int32 logread = 0;
string SceneDevice_ID;
char temp_buf[2048];

extern char config_path[1024];
extern char log_path[1024];
extern char command_path[1024];

/*Get Things type made by ArulSankar*/
const char *ThingType(string GetProductName, int32 NodeID)
{
	bool CheckThing = (strstr(GetProductName.c_str(), "Dual Relay") != NULL || strstr(GetProductName.c_str(), "Micro Double Switch") != NULL ||
		strstr(GetProductName.c_str(), "Micro Switch") != NULL || strstr(GetProductName.c_str(), "Nano Switch") != NULL ||
		strstr(GetProductName.c_str(), "Energy Switch") != NULL || strstr(GetProductName.c_str(), "Power Switch") != NULL ||
		strstr(GetProductName.c_str(), "1Pole Switch") != NULL || strstr(GetProductName.c_str(), "2Pole Switch") != NULL ||
		strstr(GetProductName.c_str(), "relay Switching") != NULL || strstr(GetProductName.c_str(), "Scene Switch") != NULL ||
		strstr(GetProductName.c_str(), "On/Off Switch") != NULL || strstr(GetProductName.c_str(), "Switch Module") != NULL ||
		strstr(GetProductName.c_str(), "Wall Switch") != NULL || strstr(GetProductName.c_str(), "Switch 3kW") != NULL ||
		strstr(GetProductName.c_str(), "Relay Switch") != NULL || strstr(GetProductName.c_str(), "Double Relay") != NULL ||
		strstr(GetProductName.c_str(), "1-Gang Switch") != NULL || strstr(GetProductName.c_str(), "2-Gang Switch") != NULL ||
		strstr(GetProductName.c_str(), "4-Gang Switch") != NULL || strstr(GetProductName.c_str(), "Plug In Switch") != NULL ||
		strstr(GetProductName.c_str(), "Plug-in Switch") != NULL || strstr(GetProductName.c_str(), "Plugin Switch") != NULL ||
		strstr(GetProductName.c_str(), "Magnet Switch") != NULL);
	if (strstr(nodeBasicStr(Manager::Get()->GetNodeBasic(homeId, NodeID)), "Controller") != NULL ||
		strstr(nodeBasicStr(Manager::Get()->GetNodeBasic(homeId, NodeID)), "Static Controller") != NULL) {
		return "Gateway Controller";
	}
	else {
		if (strstr(GetProductName.c_str(), "Door/Window") != NULL ||
			strstr(GetProductName.c_str(), "Door Window") != NULL ||
			strstr(GetProductName.c_str(), "Door Sensor") != NULL ||
			strstr(GetProductName.c_str(), "Door Opening Sensor") != NULL) {
			return "Door/Window Sensor";
		}
		else if (strstr(GetProductName.c_str(), "Thermostat") != NULL ||
					strstr(GetProductName.c_str(), "thermostat") != NULL) {
			return "Thermostat";
		}
		else if (strstr(GetProductName.c_str(), "Siren/Strobe") != NULL ||
			strstr(GetProductName.c_str(), "Siren") != NULL) {
			return "Siren";
		}
		else if (strstr(GetProductName.c_str(), "PIR Motion") != NULL ||
			strstr(GetProductName.c_str(), "Motion Detector") != NULL ||
			strstr(GetProductName.c_str(), "PIR/Motion") != NULL ||
			strstr(GetProductName.c_str(), "Motion Sensor") != NULL) {
			return "Motion Sensor";
		}
		else if (CheckThing) {
			return "On/Off Switch";
		}
		else if (strstr(GetProductName.c_str(), "Dimmer") != NULL ||
			strstr(GetProductName.c_str(), "Dimming") != NULL) {
			return "Dimmer Controller";
		}
		else if (strstr(GetProductName.c_str(), "Fixture Module") != NULL) {
			return "Fixture Module";
		}
		else if (strstr(GetProductName.c_str(), "Smart Switch") != NULL) {
			return "Socket";
		}
		else if (strstr(GetProductName.c_str(), "Lamp") != NULL) {
			return "On/Off Bulbholder";
		}
		else if (strstr(GetProductName.c_str(), "Appliance Module") != NULL) {
			return "On/Off Appliance Module";
		}
		else if (strstr(GetProductName.c_str(), "LED Strip") != NULL ||
			strstr(GetProductName.c_str(), "RGBW LED") != NULL ||
			strstr(GetProductName.c_str(), "RGBW") != NULL) {
			return "RGBW Controller";
		}
		else if (strstr(GetProductName.c_str(), "Smart LED") != NULL) {
			return "On/Off Light";
		}
		else if (strstr(GetProductName.c_str(), "Dimmable LED") != NULL) {
			return "Dimmer Light Controller";
		}
		else if (strstr(GetProductName.c_str(), "Water") != NULL ||
			strstr(GetProductName.c_str(), "Flood") != NULL) {
			return "Water/Flood Sensor";
		}
		else if (strstr(GetProductName.c_str(), "Smoke") != NULL) {
			return "Smoke Sensor";
		}
		else if (strstr(GetProductName.c_str(), "Lock") != NULL || strstr(GetProductName.c_str(), "Danalock") != NULL ||
			strstr(GetProductName.c_str(), "Polylock") != NULL || strstr(GetProductName.c_str(), "Yale") != NULL) {
			return "Locks";
		}
		return "Unknown thing";
	}
}
/*End Things type*/

/*
 * web_send_data
 * Send internal HTML string
 */
static int web_send_data (struct MHD_Connection *connection, const char *data,
		const int code, bool free, bool copy, const char *ct)
{
	struct MHD_Response *response;
	int ret;

	if (!copy && ! free)
		response = MHD_create_response_from_buffer(strlen(data), (void *) data, MHD_RESPMEM_PERSISTENT);
	else if (copy)
		response = MHD_create_response_from_buffer(strlen(data), (void *) data, MHD_RESPMEM_MUST_COPY);
	else
		response = MHD_create_response_from_buffer(strlen(data), (void *) data, MHD_RESPMEM_MUST_FREE);

	if (response == NULL)
		return MHD_NO;
	if (ct != NULL)
		MHD_add_response_header(response, "Content-type", ct);
	ret = MHD_queue_response(connection, code, response);
	MHD_destroy_response(response);
	return ret;
}

/*
 * web_read_file
 * Read files and send them out
 */
ssize_t web_read_file (void *cls, uint64_t pos, char *buf, size_t max)
{
	FILE *file = (FILE *)cls;
	fseek(file, pos, SEEK_SET);
	return fread(buf, 1, max, file);
}

/*
 * web_close_file
 */
void web_close_file (void *cls)
{
	FILE *fp = (FILE *)cls;

	fclose(fp);
}

/*
 * web_send_file
 * Read files and send them out
 */
int web_send_file (struct MHD_Connection *conn, const char *filename, const int code, const bool unl)
{
	struct stat buf;
	FILE *fp;
	struct MHD_Response *response;
	const char *p;
	const char *ct = NULL;
	int ret;
	if ((p = strchr(filename, '.')) != NULL) {
		p++;
		if (strcmp(p, "json") == 0)
			ct = "application/json";
		else if (strcmp(p, "js") == 0)
			ct = "text/javascript";
	}
	if (stat(filename, &buf) == -1 ||
			((fp = fopen(filename, "rb")) == NULL)) {
		
		if (strcmp(p, "json") == 0) {
			response = MHD_create_response_from_buffer(0, (void *) "", MHD_RESPMEM_PERSISTENT);
		}
		else {
			int len = strlen(FNF) + strlen(filename) - 1; // len(%s) + 1 for \0
			char *s = (char *)malloc(len);
			if (s == NULL) {
				fprintf(stderr, "Out of memory FNF\n");
				exit(1);
			}
			snprintf(s, len, FNF, filename);
			response = MHD_create_response_from_buffer(len, (void *) s, MHD_RESPMEM_MUST_FREE); // free
		}
	}
	else
	{
		response = MHD_create_response_from_callback(buf.st_size, 32  * 1024, &web_read_file, fp,
			&web_close_file);
	}
	if (response == NULL)
		return MHD_YES;
	if (ct != NULL) {
		MHD_add_response_header(response, "Content-type", ct);
		fprintf(stderr, ct);
	}
	ret = MHD_queue_response(conn, code, response);
	MHD_destroy_response(response);
	if (unl)
		_unlink(filename);
	return ret;
}

/*
 * web_get_groups
 * Return some XML to carry node group associations
 */

void Webserver::web_get_groups (int n, TiXmlElement *ep)
{
	int cnt = nodes[n]->numGroups();
	int i;

	TiXmlElement* groupsElement = new TiXmlElement("groups");
	ep->LinkEndChild(groupsElement);
	groupsElement->SetAttribute("cnt", cnt);
	for (i = 1; i <= cnt; i++) {
		TiXmlElement* groupElement = new TiXmlElement("group");
		MyGroup *p = nodes[n]->getGroup(i);
		groupElement->SetAttribute("ind", i);
		groupElement->SetAttribute("max", p->max);
		groupElement->SetAttribute("label", p->label.c_str());
		string str = "";
		for (unsigned int j = 0; j < p->grouplist.size(); j++) {
			str += p->grouplist[j];
			if (j + 1 < p->grouplist.size())
				str += ",";
		}
		TiXmlText *textElement = new TiXmlText(str.c_str());
		groupElement->LinkEndChild(textElement);
		groupsElement->LinkEndChild(groupElement);
	}
}

/*
 * web_get_values
 * Retrieve class values based on genres
 */
void Webserver::web_get_values (int i, TiXmlElement *ep)
{
	uint16 idcnt = nodes[i]->getValueCount();

	for (uint16 j = 0; j < idcnt; j++) {
		TiXmlElement* valueElement = new TiXmlElement("value");
		MyValue *vals = nodes[i]->getValue(j);
		ValueID id = vals->getId();
		valueElement->SetAttribute("genre", valueGenreStr(id.GetGenre()));
		valueElement->SetAttribute("type", valueTypeStr(id.GetType()));
		valueElement->SetAttribute("class", cclassStr(id.GetCommandClassId()));
		valueElement->SetAttribute("instance", id.GetInstance());
		valueElement->SetAttribute("index", id.GetIndex());
		valueElement->SetAttribute("label", Manager::Get()->GetValueLabel(id).c_str());
		valueElement->SetAttribute("units", Manager::Get()->GetValueUnits(id).c_str());
		valueElement->SetAttribute("readonly", Manager::Get()->IsValueReadOnly(id) ? "true" : "false");
		if (id.GetGenre() != ValueID::ValueGenre_Config)
			valueElement->SetAttribute("polled", Manager::Get()->isPolled(id) ? "true" : "false");
		if (id.GetType() == ValueID::ValueType_List) {
			vector<string> strs;
			Manager::Get()->GetValueListItems(id, &strs);
			valueElement->SetAttribute("count", strs.size());
			string str;
			Manager::Get()->GetValueListSelection(id, &str);
			valueElement->SetAttribute("current", str.c_str());
			for (vector<string>::iterator it = strs.begin(); it != strs.end(); it++) {
				TiXmlElement* itemElement = new TiXmlElement("item");
				valueElement->LinkEndChild(itemElement);
				TiXmlText *textElement = new TiXmlText((*it).c_str());
				itemElement->LinkEndChild(textElement);
			}
		} else {
			string str;
			TiXmlText *textElement;
			if (Manager::Get()->GetValueAsString(id, &str))
				textElement = new TiXmlText(str.c_str());
			else
				textElement = new TiXmlText("");
			if (id.GetType() == ValueID::ValueType_Decimal) {
				uint8 precision;
				if (Manager::Get()->GetValueFloatPrecision(id, &precision))
					fprintf(stderr, "node = %d id = %d value = %s precision = %d\n", i, j, str.c_str(), precision);
			}
			valueElement->LinkEndChild(textElement);
		}

		string str = Manager::Get()->GetValueHelp(id);
		if (str.length() > 0) {
			TiXmlElement* helpElement = new TiXmlElement("help");
			TiXmlText *textElement = new TiXmlText(str.c_str());
			helpElement->LinkEndChild(textElement);
			valueElement->LinkEndChild(helpElement);
		}
		ep->LinkEndChild(valueElement);
	}
}

/*
 * SendTopoResponse
 * Process topology request and return appropiate data
 */

const char *Webserver::SendTopoResponse (struct MHD_Connection *conn, const char *fun,
		const char *arg1, const char *arg2, const char *arg3)
{
	TiXmlDocument doc;
	char str[16];
	static char fntemp[32];
	char *fn;
	unsigned int i, j, k;
	uint8 cnt;
	uint32 len;
	uint8 *neighbors;
	TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "utf-8", "" );
	doc.LinkEndChild(decl);
	TiXmlElement* topoElement = new TiXmlElement("topo");
	doc.LinkEndChild(topoElement);

	if (strcmp(fun, "load") == 0) {
		cnt = MyNode::getNodeCount();
		i = 0;
		j = 1;
		while (j <= cnt && i < MAX_NODES) {
			if (nodes[i] != NULL) {
				len = Manager::Get()->GetNodeNeighbors(homeId, i, &neighbors);
				if (len > 0) {
					TiXmlElement* nodeElement = new TiXmlElement("node");
					snprintf(str, sizeof(str), "%d", i);
					nodeElement->SetAttribute("id", str);
					string list = "";
					for (k = 0; k < len; k++) {
						snprintf(str, sizeof(str), "%d", neighbors[k]);
						list += str;
						if (k < (len - 1))
							list += ",";
					}
					fprintf(stderr, "topo: node=%d %s\n", i, list.c_str());
					TiXmlText *textElement = new TiXmlText(list.c_str());
					nodeElement->LinkEndChild(textElement);
					topoElement->LinkEndChild(nodeElement);
					delete [] neighbors;
				}
				j++;
			}
			i++;
		}
	}
	//memset(temp_buf, 0, 2048);
	snprintf(temp_buf, sizeof(temp_buf), "%sozwcp.topo.XXXXXX", command_path);
	strncpy(fntemp, temp_buf, sizeof(fntemp));
	//strncpy(fntemp, "E:/ozwcp.topo.XXXXXX", sizeof(fntemp));
	//fn = mktemp(fntemp);
	if (fn == NULL)
		return EMPTY;
	strncat(fntemp, ".xml", sizeof(fntemp));
	if (debug)
		doc.Print(stdout, 0);
	doc.SaveFile(fn);
	return fn;
}

static TiXmlElement *newstat (char const *tag, char const *label, uint32 const value)
{
	char str[32];

	TiXmlElement* statElement = new TiXmlElement(tag);
	statElement->SetAttribute("label", label);
	snprintf(str, sizeof(str), "%d", value);
	TiXmlText *textElement = new TiXmlText(str);
	statElement->LinkEndChild(textElement);
	return statElement;
}

static TiXmlElement *newstat (char const *tag, char const *label, char const *value)
{
	TiXmlElement* statElement = new TiXmlElement(tag);
	statElement->SetAttribute("label", label);
	TiXmlText *textElement = new TiXmlText(value);
	statElement->LinkEndChild(textElement);
	return statElement;
}

/*
 * SendStatResponse
 * Process statistics request and return appropiate data
 */

const char *Webserver::SendStatResponse (struct MHD_Connection *conn, const char *fun,
		const char *arg1, const char *arg2, const char *arg3)
{
	TiXmlDocument doc;
	static char fntemp[32];
	char *fn;

	TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "utf-8", "" );
	doc.LinkEndChild(decl);
	TiXmlElement* statElement = new TiXmlElement("stats");
	doc.LinkEndChild(statElement);

	if (strcmp(fun, "load") == 0) {
		struct Driver::DriverData data;
		int i, j;
		int cnt;
		char str[16];

		Manager::Get()->GetDriverStatistics(homeId, &data);

		TiXmlElement* errorsElement = new TiXmlElement("errors");
		errorsElement->LinkEndChild(newstat("stat", "ACK Waiting", data.m_ACKWaiting));
		errorsElement->LinkEndChild(newstat("stat", "Read Aborts", data.m_readAborts));
		errorsElement->LinkEndChild(newstat("stat", "Bad Checksums", data.m_badChecksum));
		errorsElement->LinkEndChild(newstat("stat", "CANs", data.m_CANCnt));
		errorsElement->LinkEndChild(newstat("stat", "NAKs", data.m_NAKCnt));
		errorsElement->LinkEndChild(newstat("stat", "Out of Frame", data.m_OOFCnt));
		statElement->LinkEndChild(errorsElement);

		TiXmlElement* countsElement = new TiXmlElement("counts");
		countsElement->LinkEndChild(newstat("stat", "SOF", data.m_SOFCnt));
		countsElement->LinkEndChild(newstat("stat", "Total Reads", data.m_readCnt));
		countsElement->LinkEndChild(newstat("stat", "Total Writes", data.m_writeCnt));
		countsElement->LinkEndChild(newstat("stat", "ACKs", data.m_ACKCnt));
		countsElement->LinkEndChild(newstat("stat", "Total Broadcasts Received", data.m_broadcastReadCnt));
		countsElement->LinkEndChild(newstat("stat", "Total Broadcasts Transmitted", data.m_broadcastWriteCnt));
		statElement->LinkEndChild(countsElement);

		TiXmlElement* infoElement = new TiXmlElement("info");
		infoElement->LinkEndChild(newstat("stat", "Dropped", data.m_dropped));
		infoElement->LinkEndChild(newstat("stat", "Retries", data.m_retries));
		infoElement->LinkEndChild(newstat("stat", "Unexpected Callbacks", data.m_callbacks));
		infoElement->LinkEndChild(newstat("stat", "Bad Routes", data.m_badroutes));
		infoElement->LinkEndChild(newstat("stat", "No ACK", data.m_noack));
		infoElement->LinkEndChild(newstat("stat", "Network Busy", data.m_netbusy));
		infoElement->LinkEndChild(newstat("stat", "Not Idle", data.m_notidle));
		infoElement->LinkEndChild(newstat("stat", "Non Delivery", data.m_nondelivery));
		infoElement->LinkEndChild(newstat("stat", "Routes Busy", data.m_routedbusy));
		statElement->LinkEndChild(infoElement);

		cnt = MyNode::getNodeCount();
		i = 0;
		j = 1;
		while (j <= cnt && i < MAX_NODES) {
			struct Node::NodeData ndata;

			if (nodes[i] != NULL) {
				Manager::Get()->GetNodeStatistics(homeId, i, &ndata);
				TiXmlElement* nodeElement = new TiXmlElement("node");
				snprintf(str, sizeof(str), "%d", i);
				nodeElement->SetAttribute("id", str);
				nodeElement->LinkEndChild(newstat("nstat", "Sent messages", ndata.m_sentCnt));
				nodeElement->LinkEndChild(newstat("nstat", "Failed sent messages", ndata.m_sentFailed));
				nodeElement->LinkEndChild(newstat("nstat", "Retried sent messages", ndata.m_retries));
				nodeElement->LinkEndChild(newstat("nstat", "Received messages", ndata.m_receivedCnt));
				nodeElement->LinkEndChild(newstat("nstat", "Received duplicates", ndata.m_receivedDups));
				nodeElement->LinkEndChild(newstat("nstat", "Received unsolicited", ndata.m_receivedUnsolicited));
				nodeElement->LinkEndChild(newstat("nstat", "Last sent message", ndata.m_sentTS.substr(5).c_str()));
				nodeElement->LinkEndChild(newstat("nstat", "Last received message", ndata.m_receivedTS.substr(5).c_str()));
				nodeElement->LinkEndChild(newstat("nstat", "Last Request RTT", ndata.m_averageRequestRTT));
				nodeElement->LinkEndChild(newstat("nstat", "Average Request RTT", ndata.m_averageRequestRTT));
				nodeElement->LinkEndChild(newstat("nstat", "Last Response RTT", ndata.m_averageResponseRTT));
				nodeElement->LinkEndChild(newstat("nstat", "Average Response RTT", ndata.m_averageResponseRTT));
				nodeElement->LinkEndChild(newstat("nstat", "Quality", ndata.m_quality));
				while (!ndata.m_ccData.empty()) {
					Node::CommandClassData ccd = ndata.m_ccData.front();
					TiXmlElement* ccElement = new TiXmlElement("commandclass");
					snprintf(str, sizeof(str), "%d", ccd.m_commandClassId);
					ccElement->SetAttribute("id", str);
					ccElement->SetAttribute("name", cclassStr(ccd.m_commandClassId));
					ccElement->LinkEndChild(newstat("cstat", "Messages sent", ccd.m_sentCnt));
					ccElement->LinkEndChild(newstat("cstat", "Messages received", ccd.m_receivedCnt));
					nodeElement->LinkEndChild(ccElement);
					ndata.m_ccData.pop_front();
				}
				statElement->LinkEndChild(nodeElement);
				j++;
			}
			i++;
		}
	}
	memset(temp_buf, 0, 2048);
	snprintf(temp_buf, sizeof(temp_buf), "%sozwcp.stat.XXXXXX", command_path);
	strncpy(fntemp, temp_buf, sizeof(fntemp));
	//fn = tempnam(fntemp, NULL);
	if (fn == NULL)
		return EMPTY;
	strncat(fntemp, ".xml", sizeof(fntemp));
	if (debug)
		doc.Print(stdout, 0);
	doc.SaveFile(fn);
	return fn;
}

/*
 * SendTestHealResponse
 * Process network test and heal requests
 */

const char *Webserver::SendTestHealResponse (struct MHD_Connection *conn, const char *fun,
		const char *arg1, const char *arg2, const char *arg3)
{
	TiXmlDocument doc;
	int node;
	int arg;
	bool healrrs = false;
	static char fntemp[32];
	char *fn;

	TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "utf-8", "" );
	doc.LinkEndChild(decl);
	TiXmlElement* testElement = new TiXmlElement("testheal");
	doc.LinkEndChild(testElement);

	if (strcmp(fun, "test") == 0 && arg1 != NULL) {
		node = atoi((char *)arg1);
		arg = atoi((char *)arg2);
		if (node == 0)
			Manager::Get()->TestNetwork(homeId, arg);
		else
			Manager::Get()->TestNetworkNode(homeId, node, arg);
	} else if (strcmp(fun, "heal") == 0 && arg1 != NULL) {
		testElement = new TiXmlElement("heal");
		node = atoi((char *)arg1);
		if (arg2 != NULL) {
			arg = atoi((char *)arg2);
			if (arg != 0)
				healrrs = true;
		}
		if (node == 0)
			Manager::Get()->HealNetwork(homeId, healrrs);
		else
			Manager::Get()->HealNetworkNode(homeId, node, healrrs);
	}
	memset(temp_buf, 0, 2048);
	snprintf(temp_buf, sizeof(temp_buf), "%stestheal.XXXXXXn", log_path);
	strncpy(fntemp, temp_buf, sizeof(fntemp));
	//fn = tempnam(fntemp, NULL);
	if (fn == NULL)
		return EMPTY;
	strncat(fntemp, ".xml", sizeof(fntemp));
	if (debug)
		doc.Print(stdout, 0);
	doc.SaveFile(fn);
	return fn;
}

/*
 * SendSceneResponse
 * Process scene request and return appropiate scene data
 */

const char *Webserver::SendSceneResponse (struct MHD_Connection *conn, const char *fun,
		const char *arg1, const char *arg2, const char *arg3)
{
	TiXmlDocument doc;
	char str[16];
	string s;
	static char fntemp[32];
	char *fn;
	int cnt;
	int i;
	uint8 sid;
	TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "utf-8", "" );
	doc.LinkEndChild(decl);
	TiXmlElement* scenesElement = new TiXmlElement("scenes");
	doc.LinkEndChild(scenesElement);

	if (strcmp(fun, "create") == 0) {
		sid = Manager::Get()->CreateScene();
		if (sid == 0) {
			fprintf(stderr, "sid = 0, out of scene ids\n");
			return EMPTY;
		}
	}
	if (strcmp(fun, "values") == 0 ||
			strcmp(fun, "label") == 0 ||
			strcmp(fun, "delete") == 0 ||
			strcmp(fun, "execute") == 0 ||
			strcmp(fun, "addvalue") == 0 ||
			strcmp(fun, "update") == 0 ||
			strcmp(fun, "remove") == 0) {
		sid = atoi((char *)arg1);
		if (strcmp(fun, "delete") == 0)
			Manager::Get()->RemoveScene(sid);
		if (strcmp(fun, "execute") == 0)
			Manager::Get()->ActivateScene(sid);
		if (strcmp(fun, "label") == 0)
			Manager::Get()->SetSceneLabel(sid, string(arg2));
		if (strcmp(fun, "addvalue") == 0 ||
				strcmp(fun, "update") == 0 ||
				strcmp(fun, "remove") == 0) {
			MyValue *val = MyNode::lookup(string(arg2));
			if (val != NULL) {
				if (strcmp(fun, "addvalue") == 0) {
					if (!Manager::Get()->AddSceneValue(sid, val->getId(), string(arg3)))
						fprintf(stderr, "AddSceneValue failure\n");
				} else if (strcmp(fun, "update") == 0) {
					if (!Manager::Get()->SetSceneValue(sid, val->getId(), string(arg3)))
						fprintf(stderr, "SetSceneValue failure\n");
				} else if (strcmp(fun, "remove") == 0) {
					if (!Manager::Get()->RemoveSceneValue(sid, val->getId()))
						fprintf(stderr, "RemoveSceneValue failure\n");
				}
			}
		}
	}
	if (strcmp(fun, "load") == 0 ||
			strcmp(fun, "create") == 0 ||
			strcmp(fun, "label") == 0 ||
			strcmp(fun, "delete") == 0) { // send all sceneids
		uint8 *sptr;
		cnt = Manager::Get()->GetAllScenes(&sptr);
		scenesElement->SetAttribute("sceneid", cnt);
		for (i = 0; i < cnt; i++) {
			TiXmlElement* sceneElement = new TiXmlElement("sceneid");
			snprintf(str, sizeof(str), "%d", sptr[i]);
			sceneElement->SetAttribute("id", str);
			s = Manager::Get()->GetSceneLabel(sptr[i]);
			sceneElement->SetAttribute("label", s.c_str());
			scenesElement->LinkEndChild(sceneElement);
		}
		delete [] sptr;
	}
	if (strcmp(fun, "values") == 0 ||
			strcmp(fun, "addvalue") == 0 ||
			strcmp(fun, "remove") == 0 ||
			strcmp(fun, "update") == 0) {
		vector<ValueID> vids;
		cnt = Manager::Get()->SceneGetValues(sid, &vids);
		scenesElement->SetAttribute("scenevalue", cnt);
		for (vector<ValueID>::iterator it = vids.begin(); it != vids.end(); it++) {
			TiXmlElement* valueElement = new TiXmlElement("scenevalue");
			valueElement->SetAttribute("id", sid);
			snprintf(str, sizeof(str), "0x%x", (*it).GetHomeId());
			valueElement->SetAttribute("home", str);
			valueElement->SetAttribute("node", (*it).GetNodeId());
			valueElement->SetAttribute("class", cclassStr((*it).GetCommandClassId()));
			valueElement->SetAttribute("instance", (*it).GetInstance());
			valueElement->SetAttribute("index", (*it).GetIndex());
			valueElement->SetAttribute("type", valueTypeStr((*it).GetType()));
			valueElement->SetAttribute("genre", valueGenreStr((*it).GetGenre()));
			valueElement->SetAttribute("label", Manager::Get()->GetValueLabel(*it).c_str());
			valueElement->SetAttribute("units", Manager::Get()->GetValueUnits(*it).c_str());
			Manager::Get()->SceneGetValueAsString(sid, *it, &s);
			TiXmlText *textElement = new TiXmlText(s.c_str());
			valueElement->LinkEndChild(textElement);
			scenesElement->LinkEndChild(valueElement);
		}
	}
	memset(temp_buf, 0, 2048);
	snprintf(temp_buf, sizeof(temp_buf), "%sozwcp.scenes.XXXXXX", command_path);
	strncpy(fntemp, temp_buf, sizeof(fntemp));
	//fn = tempnam(fntemp, NULL);
	if (fn == NULL)
		return EMPTY;
	strncat(fntemp, ".xml", sizeof(fntemp));
	if (debug)
		doc.Print(stdout, 0);
	doc.SaveFile(fn);
	return fn;
}

/*
 * SendPollResponse
 * Process poll request from client and return
 * data as xml.
 */
int Webserver::SendPollResponse (struct MHD_Connection *conn)
{
	system("del /q/f/s %TEMP%\\*.xml");
	//system("del /Q C:\\ProgramData\\QuikAV\\IOT\\Command\\*.xml");
	TiXmlDocument doc;
	struct stat buf;
	const int logbufsz = 1024;	// max amount to send of log per poll
	char logbuffer[logbufsz+1];
	off_t bcnt;
	char str[16];
	int32 i, j;
	//int32 logread = 0;
	char fntemp[512];
	char *fn;
	FILE *fp;
	int32 ret;

	TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "utf-8", "" );
	doc.LinkEndChild(decl);
	TiXmlElement* pollElement = new TiXmlElement("poll");
	doc.LinkEndChild(pollElement);
	if (homeId != 0L)
		snprintf(str, sizeof(str), "%08x", homeId);
	else
		str[0] = '\0';
	pollElement->SetAttribute("homeid", str);
	if (nodeId != 0)
		snprintf(str, sizeof(str), "%d", nodeId);
	else
		str[0] = '\0';
	pollElement->SetAttribute("nodeid", str);
	snprintf(str, sizeof(str), "%d", SUCnodeId);
	pollElement->SetAttribute("sucnodeid", str);
	pollElement->SetAttribute("nodecount", MyNode::getNodeCount());
	//pollElement->SetAttribute("cmode", cmode);
	pollElement->SetAttribute("save", needsave);
	pollElement->SetAttribute("noop", noop);
	if (noop)
		noop = false;
	bcnt = logbytes;

	char OZW_Log[2048];
	memset(OZW_Log, 0, 2048);
	snprintf(OZW_Log, sizeof(OZW_Log), "%sOZW_Log.txt", command_path);

	if (stat(OZW_Log, &buf) != -1 &&
			buf.st_size > bcnt &&
			(fp = fopen(OZW_Log, "r")) != NULL) {
		if (fseek(fp, bcnt, SEEK_SET) != -1) {
			logread = fread(logbuffer, 1, logbufsz, fp);
			while (logread > 0 && logbuffer[--logread] != '\n')
				;
			logbytes = bcnt + logread;
			fclose(fp);
		}
	}
	logbuffer[logread] = '\0';

	TiXmlElement* logElement = new TiXmlElement("log");
	pollElement->LinkEndChild(logElement);
	logElement->SetAttribute("size", logread);
	logElement->SetAttribute("offset", logbytes - logread);
	TiXmlText *textElement = new TiXmlText(logbuffer);
	logElement->LinkEndChild(textElement);

	TiXmlElement* adminElement = new TiXmlElement("admin");
	pollElement->LinkEndChild(adminElement);
	adminElement->SetAttribute("active", getAdminState() ? "true" : "false");
	if (adminmsg.length() > 0) {
		msg = getAdminFunction() + getAdminMessage();
		
		TiXmlText *textElement = new TiXmlText(msg.c_str());
		adminElement->LinkEndChild(textElement);
		adminmsg.clear();
	}

	TiXmlElement* updateElement = new TiXmlElement("update");
	pollElement->LinkEndChild(updateElement);
	i = MyNode::getRemovedCount();
	if (i > 0) {
		logbuffer[0] = '\0';
		while (i > 0) {
			uint8 node = MyNode::getRemoved();
			snprintf(str, sizeof(str), "%d", node);
			strcat(logbuffer, str);
			i = MyNode::getRemovedCount();
			if (i > 0)
				strcat(logbuffer, ",");
		}
		updateElement->SetAttribute("remove", logbuffer);
	}

	//if (logread == 0) {
		//OBoxlog();
		//Sleep(100);
	//}

	pthread_mutex_lock(&nlock);
	if (MyNode::getAnyChanged()) {
		i = 0;
		j = 1;
		//OBoxlog();
		while (j <= MyNode::getNodeCount() && i < MAX_NODES) {
			if (nodes[i] != NULL && nodes[i]->getChanged()) {
				bool listening;
				bool flirs;
				bool zwaveplus;
				TiXmlElement* nodeElement = new TiXmlElement("node");
				pollElement->LinkEndChild(nodeElement);
				nodeElement->SetAttribute("id", i);
				zwaveplus = Manager::Get()->IsNodeZWavePlus(homeId, i);
				if (zwaveplus) {
					string value = Manager::Get()->GetNodePlusTypeString(homeId, i);
					value += " " + Manager::Get()->GetNodeRoleString(homeId, i);
					nodeElement->SetAttribute("btype", value.c_str());
					nodeElement->SetAttribute("gtype", Manager::Get()->GetNodeDeviceTypeString(homeId, i).c_str());
				} else {
					nodeElement->SetAttribute("btype", nodeBasicStr(Manager::Get()->GetNodeBasic(homeId, i)));
					nodeElement->SetAttribute("gtype", Manager::Get()->GetNodeType(homeId, i).c_str());
				}
				nodeElement->SetAttribute("name", Manager::Get()->GetNodeName(homeId, i).c_str());
				nodeElement->SetAttribute("location", Manager::Get()->GetNodeLocation(homeId, i).c_str());
				nodeElement->SetAttribute("manufacturer", Manager::Get()->GetNodeManufacturerName(homeId, i).c_str());
				nodeElement->SetAttribute("product", Manager::Get()->GetNodeProductName(homeId, i).c_str());
				listening = Manager::Get()->IsNodeListeningDevice(homeId, i);
				nodeElement->SetAttribute("listening", listening ? "true" : "false");
				flirs = Manager::Get()->IsNodeFrequentListeningDevice(homeId, i);
				nodeElement->SetAttribute("frequent", flirs ? "true" : "false");
				nodeElement->SetAttribute("zwaveplus", zwaveplus ? "true" : "false");
				nodeElement->SetAttribute("beam", Manager::Get()->IsNodeBeamingDevice(homeId, i) ? "true" : "false");
				nodeElement->SetAttribute("routing", Manager::Get()->IsNodeRoutingDevice(homeId, i) ? "true" : "false");
				nodeElement->SetAttribute("security", Manager::Get()->IsNodeSecurityDevice(homeId, i) ? "true" : "false");
				nodeElement->SetAttribute("time", nodes[i]->getTime());
#if  0
				fprintf(stderr, "i=%d failed=%d\n", i, Manager::Get()->IsNodeFailed(homeId, i));
				fprintf(stderr, "i=%d awake=%d\n", i, Manager::Get()->IsNodeAwake(homeId, i));
				fprintf(stderr, "i=%d state=%s\n", i, Manager::Get()->GetNodeQueryStage(homeId, i).c_str());
				fprintf(stderr, "i=%d listening=%d flirs=%d\n", i, listening, flirs);
#endif
				if (Manager::Get()->IsNodeFailed(homeId, i))
					nodeElement->SetAttribute("status", "Dead");
				else {
					string s = Manager::Get()->GetNodeQueryStage(homeId, i);
					if (s == "Complete") {
						if (i != nodeId && !listening && !flirs)
							nodeElement->SetAttribute("status", Manager::Get()->IsNodeAwake(homeId, i) ? "Awake" : "Sleeping" );
						else
							nodeElement->SetAttribute("status", "Ready");
					} else {
						if (i != nodeId && !listening && !flirs)
							s = s + (Manager::Get()->IsNodeAwake(homeId, i) ? " (awake)" : " (sleeping)");
						nodeElement->SetAttribute("status", s.c_str());
					}
				}
				web_get_groups(i, nodeElement);
				// Don't think the UI needs these
				//web_get_genre(ValueID::ValueGenre_Basic, i, nodeElement);
				web_get_values(i, nodeElement);
				nodes[i]->setChanged(false);
				j++;
			}
			i++;
		}
	}
	pthread_mutex_unlock(&nlock);
	memset(temp_buf, 0, 2048);
	snprintf(temp_buf, sizeof(temp_buf), "%sozwcp.poll.XXXXXX", command_path);
	strncpy(fntemp, temp_buf, sizeof(fntemp));
	fn = tmpnam(fntemp);
	fprintf(stderr, "fn path is:%s\n", fntemp);
	fprintf(stderr, "XML path is:%s\n", fn);
	if (fn == NULL)
		return MHD_YES;
	strncat(fntemp, ".xml", sizeof(fntemp));
	if (debug)
		doc.Print(stdout, 0);
	doc.SaveFile(fn);
	ret = web_send_file(conn, fn, MHD_HTTP_OK, true);
	//system("del /Q C:\\ProgramData\\QuikAV\\IOT\\Command\\*.xml");
	//system("del /q/f/s %TEMP%\\*.xml");
	return ret;
}
/**
  * Func : SendDeviceID_userdata
  * arg  : struct MHD_Connection *conn
  * ret  : No return value
  * Desc : 
*/

/*ArulSankar API*/

int Webserver::SendDeviceID_userdata(struct MHD_Connection *conn)
{
	char OBox[32];
	const char* Command = NULL;
	const char* DeviceID = NULL;
	const char* Device_Rename = NULL;
	const char* Action = NULL;
	const char* T_Mode = NULL;
	const char* T_Clock = NULL;
	const char*  T_Value = NULL;
	const char* Log = NULL;
	char str_nodes[16];
	int32 i = 0, j = 0, z = 1, ret;
	int32 nodeID = 0, nodeID_new = 0, Sensors = 0, BrightDim = 0;
	char *fn = NULL;

	char fntemp[512];
	std::ofstream file_id;
	std::ofstream file_log;
	Json::Value event_status;
	Json::Value event_base_scene;
	Json::Value event_base_scene_time_based;
	Json::Value event_target_scene;
	Json::Value Devices_status;
	Json::Value Devices_status_properties;
	Json::Value Devices_status_properties_target;
	Json::StyledWriter styledWriter;
	string device_command;
	string CClass;
	string CDevice_ID;
	Action = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "newtargetvalue");
	Command = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "action");
	DeviceID = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "deviceid");
	Device_Rename = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "name");
	T_Mode = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "mode");
	T_Clock = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "date");
	T_Value = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "value");
	Log = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "save");
	if (Command != NULL && Command[0] != '\0')
	{
		system("del /q/f/s %TEMP%\\*.json");
		/*if (logread != 0){
		Devices_status["help"] = "Sorry your OBox is busy mode, please come back after sometimes !!!!!";
		Devices_status["status_code"] = 600;
		goto event_status_zwave;
		}*/
		//system("del /Q C:\\ProgramData\\QuikAV\\IOT\\Command\\*.json");
#if 1
		if (getAdminState()) {
			if (strcmp(Command, "cancel") == 0) {
				Manager::Get()->CancelControllerCommand(homeId);
				setAdminState(false);
				if (!getAdminState()) {
					snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
					Devices_status["controller_status"] = OBox;
					Devices_status["status_code"] = MHD_HTTP_OK;
					Devices_status["help"] = msg.c_str();
				}Sleep(1000);
				if (Event_ZWave == 7) {
					Event_ZWave = 2;
					snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
					Devices_status["controller_status"] = OBox;
					Devices_status["status_code"] = MHD_HTTP_OK;
					Devices_status["help"] = "Add Device: command was cancelled.";
					goto event_status_zwave;
				}
				else {
					Sleep(1000);
					snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
					Devices_status["controller_status"] = OBox;
					Devices_status["status_code"] = MHD_HTTP_OK;
					Devices_status["help"] = msg.c_str();
				}
			}
			else {
				if (getAdminState()) {
					Devices_status["help"] = "Sorry your OBox is admin mode, please do cancel all Z-Wave commands using (cancel command)!";
				}
				else {
					Devices_status["help"] = "Sorry your OBox is busy mode, please come back after sometimes !";
				}
				//Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["status_code"] = 600;
			}
			if (Log != NULL && Log[0] != '\0') {
				if (strcmp(Log, "YES") == 0 || strcmp(Log, "NO") == 0) {
					goto exitLoop;
				}
			}
			event_status["Controller_Command"] = Devices_status;
			snprintf(temp_buf, sizeof(temp_buf), "%sozwcp.device_action.XXXXXX", command_path);
			strncpy(fntemp, temp_buf, sizeof(fntemp));
			//fn = tempnam(fntemp, NULL);
			fn = tmpnam(fntemp);
			//fn = tempnam(fntemp, NULL);
			if (fn == NULL)
				return MHD_YES;
			strncat(fntemp, ".json", sizeof(fntemp));
			file_id.open(fn);
			file_id << styledWriter.write(event_status);
			file_id.close();

			ret = web_send_file(conn, fn, 600, true);
			fprintf(stderr, "IP is : %s ", IP);
			//ret = web_send_file(conn, EMPTY, 600, false, false, NULL);
			//system("del /Q C:\\ProgramData\\QuikAV\\IOT\\Command\\*.json");
			//free(fn);
			return ret;
		}
#endif
		exitLoop:
		Devices_status.clear();
		if (strcmp(Command, "GatewayStatus") == 0) {
			//fprintf(stderr, "time=%d \n", nodes[1]->getTime());
			Devices_status["status_code"] = MHD_HTTP_OK;
			goto event_status_zwave;
		}
		else if (strcmp(Command, "cancel") == 0) {
			Manager::Get()->CancelControllerCommand(homeId);
			setAdminState(false);
			if (!getAdminState()) {
				snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
				Devices_status["controller_status"] = OBox;
				Devices_status["status_code"] = MHD_HTTP_OK;
				Devices_status["help"] = msg.c_str();
			}Sleep(2000);
			if (Event_ZWave == 7) {
				Event_ZWave = 2;
				snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
				Devices_status["controller_status"] = OBox;
				Devices_status["status_code"] = MHD_HTTP_OK;
				Devices_status["help"] = "Add Device: command was cancelled.";
				//Sleep(1000);
				goto event_status_zwave;
			}
			else {
				Sleep(1000);
				snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
				Devices_status["controller_status"] = OBox;
				Devices_status["status_code"] = MHD_HTTP_OK;
				Devices_status["help"] = msg.c_str();
			}
		}
		else if (strcmp(Command, "addds") == 0) {
			setAdminFunction("Add Device");
			setAdminState(Manager::Get()->AddNode(homeId, true));
			Sleep(2000);
			if (getAdminState()) {

				snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
				Devices_status["controller_status"] = OBox;
				Devices_status["status_code"] = MHD_HTTP_OK;
				Devices_status["help"] = msg.c_str();
			}
			else {
				snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
				Devices_status["controller_status"] = OBox;
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = msg.c_str();
			}
		}
		else if (strcmp(Command, "remd") == 0) {
			setAdminFunction("Remove Device");
			setAdminState(Manager::Get()->RemoveNode(homeId));
			Sleep(2000);
			if (getAdminState()) {

				snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
				Devices_status["controller_status"] = OBox;
				Devices_status["status_code"] = MHD_HTTP_OK;
				Devices_status["help"] = msg.c_str();
			}
			else {
				snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
				Devices_status["controller_status"] = OBox;
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = msg.c_str();
			}
		}
		else if (strcmp(Command, "remfn") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0') {
				Devices_status.clear();
				int32 nodeID = atoi(DeviceID);
				if (nodeID < MAX_NODES && nodes[nodeID] != NULL) {
					setAdminFunction("Remove Failed Node");
					setAdminState(Manager::Get()->RemoveFailedNode(homeId, nodeID));
					Sleep(2000);
					if (getAdminState()) {
						snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
						Devices_status["controller_status"] = OBox;
						Devices_status["status_code"] = MHD_HTTP_OK;
						//Devices_status["help"] = msg.c_str();
					}
					else {
						snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
						Devices_status["controller_status"] = OBox;
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
						//Devices_status["help"] = msg.c_str();
					}
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
			}
			else {
				string str_error = "Please check your deviceid option!!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = str_error.c_str();
			}
		}
		else if (strcmp(Command, "rename") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0') {
				Devices_status.clear();
				int32 nodeID = atoi(DeviceID);
				if (nodeID < MAX_NODES && nodes[nodeID] != NULL) {
					//Device_rename.clear();
					if (Device_Rename != NULL && Device_Rename[0] != '\0') {
						Manager::Get()->SetNodeName(homeId, nodeID, Device_Rename);
						Manager::Get()->WriteConfig(homeId);
						pthread_mutex_lock(&glock);
						needsave = false;
						pthread_mutex_unlock(&glock);
						Devices_status["status"] = "Your device name was renamed.";
						Devices_status["status_code"] = MHD_HTTP_OK;
					}
					else {
						Devices_status["status"] = "Please check rename option.";
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					}
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
			}
			else {
				string str_error = "Please check your deviceid option!!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = str_error.c_str();
			}
		}
		else if (strcmp(Command, "reset") == 0) { /* reset */
			Devices_status.clear();
			char str_reset[512];
			snprintf(str_reset, sizeof(str_reset), "%szwcfg_0x%08x.xml", command_path, homeId);
			Manager::Get()->ResetController(homeId);
			//snprintf(str_reset, sizeof(str_reset), "zwcfg_0x%08x.xml", homeId);
			MyNode::resetnodecount();
			remove(str_reset);
			memset(str_reset, 0, 512);
			snprintf(str_reset, sizeof(str_reset), "%szwscene.xml", command_path);
			remove(str_reset);
			memset(str_reset, 0, 512);
			snprintf(str_reset, sizeof(str_reset), "%sscene.json", command_path);
			remove(str_reset);
			Devices_status["status"] = "Your gateway was reset.";
			Devices_status["status_code"] = MHD_HTTP_OK;
		}
		else if (strcmp(Command, "gateway_status") == 0) {
			Devices_status.clear();
			Devices_status["gateway_status_code"] = Event_ZWave;
			Devices_status["status"] = "See your gateway status.";
			Devices_status["status_code"] = MHD_HTTP_OK;
		}
		else if (strcmp(Command, "gateway_refresh") == 0) { /* refresh gateway */
			if (DeviceID != NULL && DeviceID[0] != '\0') {
				Devices_status.clear();
				int32 nodeID = atoi(DeviceID);
				//while (nodeID < MAX_NODES)
				//{
				if (nodes[nodeID] != NULL)
				{
					Manager::Get()->RefreshNodeInfo(homeId, nodeID);
					//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
					Devices_status["status"] = "Your device is refreshed.";
					Devices_status["status_code"] = MHD_HTTP_OK;
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
				//i++;
				//}
			}
			else {
				string str_error = "Please check your deviceid option!!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = str_error.c_str();
			}
		}
		else if (strcmp(Command, "scene_data_time") == 0) { /* Time based scene data format */
			Devices_status.clear();
			event_status.clear();
			Devices_status_properties_target["nodeid"] = 0;
			Devices_status_properties_target["value"] = "True/False";
			event_target_scene.append(Devices_status_properties_target);
			Devices_status_properties_target.clear();
			Devices_status_properties["repeat"] = "none";
			Devices_status_properties["minutes"] = tm.tm_min;
			Devices_status_properties["seconds"] = tm.tm_sec;
			Devices_status_properties["hours"] = tm.tm_hour;
			Devices_status_properties["month_day"] = tm.tm_mday;
			Devices_status_properties["month"] = tm.tm_mon + 1;
			Devices_status_properties["year"] = tm.tm_year + 1900;
			Devices_status_properties["year_day"] = tm.tm_yday;
			//Devices_status_properties["time_zone"] = tm.tm_zone;
			event_base_scene.append(Devices_status_properties);
			//event_base_scene_time_based.append(Devices_status_properties_time_based);
			Devices_status_properties.clear();
			//Devices_status_properties_time_based.clear();
			event_status["time_based_thing"] = event_base_scene;
			//event_status["time_based_thing_non_repeat"] = event_base_scene_time_based;
			event_status["target_thing"] = event_target_scene;
			Devices_status["status_code"] = MHD_HTTP_OK;
			goto event_status_zwave_end;
		}
		else if (strcmp(Command, "scene_data") == 0) { /* Scene data format */
			Devices_status.clear();
			event_status.clear();
			Devices_status_properties_target["nodeid"] = 0;
			Devices_status_properties_target["value"] = "True/False";
			event_target_scene.append(Devices_status_properties_target);
			Devices_status_properties_target.clear();
			Devices_status_properties["nodeid"] = 0;
			Devices_status_properties["value"] = "True/False";
			event_base_scene.append(Devices_status_properties);
			Devices_status_properties.clear();
			event_status["base_thing"] = event_base_scene;
			event_status["target_thing"] = event_target_scene;
			Devices_status["status_code"] = MHD_HTTP_OK;
			goto event_status_zwave_end;
		}
		else if (strcmp(Command, "settarget") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0' && Action != NULL && Action[0] != '\0') {
				Devices_status.clear();
				/*if (logread != 0){
				Devices_status["help"] = "Sorry your OBox is busy mode, please come back after sometimes !!!!!";
				Devices_status["status_code"] = 600;
				goto event_status_zwave;
				}*/
				int32 nodeID = atoi(DeviceID);
				if (nodeID < MAX_NODES) {
					if (nodes[nodeID] != NULL && nodeID != 1) {
						if (!Manager::Get()->IsNodeFailed(homeId, nodeID)){
						MyValue *vals = nodes[nodeID]->getValue(0);
						string Action_str;
						ValueID id = vals->getId();
						if (Manager::Get()->GetValueAsString(id, &Action_str))
						{
							Action_str = Action_str.c_str();
						}
						if (strcmp(Action, Action_str.c_str()) == 0) {
							Devices_status["help"] = "Sorry your device already in following state";
							if (strcmp(Action, "True") == 0) {
								Devices_status["state"] = "On";
							}
							else if (strcmp(Action, "False") == 0) {
								Devices_status["state"] = "Off";
							}
							else {
								Devices_status["state"] = Action_str.c_str();
							}
							Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
							goto event_status_zwave;
						}
						if (Action != NULL && Action[0] != '\0') {
							if (strcmp(Action, "True") == 0 || strcmp(Action, "False") == 0) {
								if (strcmp(cclassStr(id.GetCommandClassId()), "SWITCH BINARY") == 0) {
									CClass = cclassStr(id.GetCommandClassId());
									CDevice_ID = DeviceID;
									//string CAction = Action;
									device_command = CDevice_ID + "-" + CClass + "-user-bool-1-0";
								}
								else {
									Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
									Devices_status["help"] = "This Device not support on this action!!!!!";
									goto event_status_zwave;
								}
								MyValue *device_action = MyNode::lookup(string(device_command));
								if (device_action != NULL) {
									string arg = Action;
									if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
										fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
										//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
										//Sleep(1);
									}
								}
								else {
									//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
								}
								Devices_status["status_code"] = MHD_HTTP_OK;
								Devices_status["On/Off"] = Action;
								string target;
								Manager::Get()->RefreshNodeInfo(homeId, nodeID);
								//Sleep(1);
							}
							else if (strcmp(Action, "Secured") == 0 || strcmp(Action, "Unsecure") == 0) {
								if (strcmp(cclassStr(id.GetCommandClassId()), "SENSOR BINARY") != 0) {
									CClass = "DOOR LOCK";
									CDevice_ID = DeviceID;
									//string CAction = Action;
									device_command = CDevice_ID + "-" + CClass + "-user-list-1-1";
								}
								else {
									Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
									Devices_status["help"] = "This Device not support on this action!!!!!";
									goto event_status_zwave;
								}
								MyValue *device_action = MyNode::lookup(string(device_command));
								if (device_action != NULL) {
									string arg = Action;
									if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
										fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
										//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
										//Sleep(1);
									}
								}
								else {
									//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
								}
								Devices_status["status_code"] = MHD_HTTP_OK;
								Devices_status["On/Off"] = Action;
								string target;
								Manager::Get()->RefreshNodeInfo(homeId, nodeID);
								//Sleep(1);
							}
							else {
								BrightDim = atoi(Action);
								fprintf(stderr, "Bright : %d\n", BrightDim);
								if (strlen(Action) < 3) {
									if (((Action[0] >= 'a' && Action[0] <= 'z') || (Action[0] >= 'A' && Action[0] <= 'Z')) ||
										((Action[1] >= 'a' && Action[1] <= 'z') || (Action[1] >= 'A' && Action[1] <= 'Z'))) {
										Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
										Devices_status["help"] = "Please check your TargetValue (0-99)!!!!!";
										goto event_status_zwave;
									}
									if (BrightDim >= 0 && BrightDim < 100) {
										if (strcmp(cclassStr(id.GetCommandClassId()), "SWITCH MULTILEVEL") == 0) {
											CClass = cclassStr(id.GetCommandClassId());
											CDevice_ID = DeviceID;
											//string CAction = Action;
											device_command = CDevice_ID + "-" + CClass + "-user-byte-1-0";
										}
										else {
											Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
											Devices_status["help"] = "This Device not support on this action!!!!!";
											goto event_status_zwave;
										}
										MyValue *device_action = MyNode::lookup(string(device_command));
										if (device_action != NULL) {
											string arg = Action;
											//fprintf(stderr, "Set Arg is=%s\n", arg.c_str());
											if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
												fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
												//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
												//Sleep(1);
											}
										}
										else {
											//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
										}
										Devices_status["status_code"] = MHD_HTTP_OK;
										Devices_status["On/Off"] = Action;

										//Sleep(1);
										string dimmer;
										Manager::Get()->RefreshNodeInfo(homeId, nodeID);
									}
									else {
										Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
										Devices_status["help"] = "Please check your TargetValue (0-99)!!!!!";
									}
								}
								else if (strlen(Action) == 6) {
									if (strcmp(cclassStr(id.GetCommandClassId()), "SENSOR BINARY") != 0) {
										CClass = "COLOR";
										CDevice_ID = DeviceID;
										//string CAction = Action;
										device_command = CDevice_ID + "-" + CClass + "-user-string-1-0";
										fprintf(stderr, "Command is:%s\n", Action);
									}
									else {
										Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
										Devices_status["help"] = "This Device not support on this action!!!!!";
										goto event_status_zwave;
									}
									MyValue *device_action = MyNode::lookup(string(device_command));
									if (device_action != NULL) {
										string arg = Action;
										arg = "#" + arg;
										fprintf(stderr, "Set Arg is=%s\n", arg.c_str());
										if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
											fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
											//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
											//Sleep(1);
										}
									}
									else {
										//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
									}
									Devices_status["status_code"] = MHD_HTTP_OK;
									Devices_status["Color_code"] = Action;
									string target;
									Manager::Get()->RefreshNodeInfo(homeId, nodeID);
									//Sleep(1);
								}
								else {
									Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
									Devices_status["help"] = "Please check your Color Codes (RRGGBB)!!!!!";
									goto event_status_zwave;
								}
							}
						}
						else {
							Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
							Devices_status["help"] = "Please check your TargetAction!!!!!";
						}
					}
					else {
						Devices_status["status_code"] = 601;
						Devices_status["help"] = "This Device is dead please check your device status!!!!!";
					}
				}
					else {
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
						Devices_status["help"] = "This Device not available please check your device status!!!!!";
					}
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
			}
			else {
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = "Please check your API Request!!!!!";
			}
		}
		else if (strcmp(Command, "setmode") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0' && T_Mode != NULL && T_Mode[0] != '\0') {
				Devices_status.clear();
				/*if (logread != 0){
				Devices_status["help"] = "Sorry your OBox is busy mode, please come back after sometimes !!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				goto event;
				}*/
				int32 nodeID = atoi(DeviceID);
				if (nodeID < MAX_NODES) {
					if (nodes[nodeID] != NULL && nodeID != 1) {
						if (T_Mode != NULL && T_Mode[0] != '\0') {
							bool thermostat_mode = (strcmp(T_Mode, "Off") == 0 || strcmp(T_Mode, "Heat") == 0 || strcmp(T_Mode, "Cool") == 0 || strcmp(T_Mode, "Auto") == 0 ||
								strcmp(T_Mode, "Aux Heat") == 0 || strcmp(T_Mode, "Resume") == 0 || strcmp(T_Mode, "Fan Only") == 0 || strcmp(T_Mode, "Furnace") == 0 ||
								strcmp(T_Mode, "Dry Air") == 0 || strcmp(T_Mode, "Moist Air") == 0 || strcmp(T_Mode, "Auto Changeover") == 0 ||
								strcmp(T_Mode, "Heat Econ") == 0 || strcmp(T_Mode, "Cool Econ") == 0 || strcmp(T_Mode, "Away") == 0);
							if (thermostat_mode) {
								CClass = "THERMOSTAT MODE";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-list-1-0";
							}
							else {
								Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
								Devices_status["help"] = "This Device not support on this action!!!!!";
								goto event_status_zwave;
							}
							MyValue *device_action = MyNode::lookup(string(device_command));
							if (device_action != NULL) {
								string arg = T_Mode;
								if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
									fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
									//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
									//Sleep(1);
								}
							}
							else {
								//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
							}
							Devices_status["status_code"] = MHD_HTTP_OK;
							Devices_status["mode"] = T_Mode;
							string target;
							Manager::Get()->RefreshNodeInfo(homeId, nodeID);
							//Sleep(1);
						}
						else {
							Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
							Devices_status["help"] = "Please check your mode selection!!!!!";
						}
					}
					else {
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
						Devices_status["help"] = "This Device not available please check your device status!!!!!";
					}
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
			}
			else {
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = "Please check your API Request!!!!!";
			}
		}
		else if (strcmp(Command, "setfanmode") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0' && T_Mode != NULL && T_Mode[0] != '\0') {
				Devices_status.clear();
				/*if (logread != 0){
				Devices_status["help"] = "Sorry your OBox is busy mode, please come back after sometimes !!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				goto event;
				}*/
				int32 nodeID = atoi(DeviceID);
				if (nodeID < MAX_NODES) {
					if (nodes[nodeID] != NULL && nodeID != 1) {
						if (T_Mode != NULL && T_Mode[0] != '\0') {
							bool thermostat_fanmode = (strcmp(T_Mode, "Auto Low") == 0 || strcmp(T_Mode, "Auto High") == 0 ||
								strcmp(T_Mode, "On Low") == 0 || strcmp(T_Mode, "On High") == 0);
							if (thermostat_fanmode) {
								CClass = "THERMOSTAT FAN MODE";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-list-1-0";
							}
							else {
								Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
								Devices_status["help"] = "This Device not support on this action!!!!!";
								goto event_status_zwave;
							}
							MyValue *device_action = MyNode::lookup(string(device_command));
							if (device_action != NULL) {
								string arg = T_Mode;
								if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
									fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
									//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
									//Sleep(1);
								}
							}
							else {
								//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
							}
							Devices_status["status_code"] = MHD_HTTP_OK;
							Devices_status["mode"] = T_Mode;
							string target;
							Manager::Get()->RefreshNodeInfo(homeId, nodeID);
							//Sleep(1);
						}
						else {
							Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
							Devices_status["help"] = "Please check your mode selection!!!!!";
						}
					}
					else {
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
						Devices_status["help"] = "This Device not available please check your device status!!!!!";
					}
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
			}
			else {
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = "Please check your API Request!!!!!";
			}
		}
		else if (strcmp(Command, "setpoint") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0' && T_Mode != NULL && T_Mode[0] != '\0' && T_Value != NULL && T_Value[0] != '\0') {
				Devices_status.clear();
				/*if (logread != 0){
				Devices_status["help"] = "Sorry your OBox is busy mode, please come back after sometimes !!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				goto event;
				}*/
				int32 nodeID = atoi(DeviceID);
				if (nodeID < MAX_NODES) {
					if (nodes[nodeID] != NULL && nodeID != 1) {
						if (T_Mode != NULL && T_Mode[0] != '\0') {
							if (strcmp(T_Mode, "Heating 1") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-1";
							}
							else if (strcmp(T_Mode, "Cooling 1") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-2";
							}
							else if (strcmp(T_Mode, "Furnace") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-7";
							}
							else if (strcmp(T_Mode, "Dry Air") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-8";
							}
							else if (strcmp(T_Mode, "Moist Air") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-9";
							}
							else if (strcmp(T_Mode, "Auto Changeover") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-10";
							}
							else if (strcmp(T_Mode, "Heating Econ") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-11";
							}
							else if (strcmp(T_Mode, "Cooling Econ") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-12";
							}
							else if (strcmp(T_Mode, "Away Heating") == 0) {
								CClass = "THERMOSTAT SETPOINT";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-decimal-1-13";
							}
							else {
								Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
								Devices_status["help"] = "This Device not support on this action!!!!!";
								goto event_status_zwave;
							}
							MyValue *device_action = MyNode::lookup(string(device_command));
							if (device_action != NULL) {
								string arg = T_Value;
								if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
									fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
									//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
									//Sleep(1);
								}
							}
							else {
								//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
							}
							Devices_status["status_code"] = MHD_HTTP_OK;
							Devices_status["mode"] = T_Mode;
							string target;
							Manager::Get()->RefreshNodeInfo(homeId, nodeID);
							//Sleep(1);
						}
						else {
							Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
							Devices_status["help"] = "Please check your mode selection!!!!!";
						}
					}
					else {
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
						Devices_status["help"] = "This Device not available please check your device status!!!!!";
					}
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
			}
			else {
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = "Please check your API Request!!!!!";
			}
		}
		else if (strcmp(Command, "setclock") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0' && T_Clock != NULL && T_Clock[0] != '\0' && T_Value != NULL && T_Value[0] != '\0') {
				Devices_status.clear();
				/*if (logread != 0){
				Devices_status["help"] = "Sorry your OBox is busy mode, please come back after sometimes !!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				goto event;
				}*/
				bool clock_set = (strcmp(T_Mode, "Monday") == 0 || strcmp(T_Mode, "Tuesday") == 0 || strcmp(T_Mode, "Wednesday") == 0 ||
					strcmp(T_Mode, "Thursday") == 0 || strcmp(T_Mode, "Friday") == 0 || strcmp(T_Mode, "Saturday") == 0 ||
					strcmp(T_Mode, "Sunday") == 0);
				int32 nodeID = atoi(DeviceID);
				int32 Value = atoi(T_Value);
				if (nodeID < MAX_NODES) {
					if (nodes[nodeID] != NULL && nodeID != 1) {
						if (T_Mode != NULL && T_Mode[0] != '\0') {
							if (strcmp(T_Mode, "Day") == 0) {
								if (clock_set) {
									CClass = "CLOCK";
									CDevice_ID = DeviceID;
									//string CAction = Action;
									device_command = CDevice_ID + "-" + CClass + "-user-byte-1-0";
								}
							}
							else if (strcmp(T_Mode, "Hour") == 0 && Value < 0 && Value > 24) {
								CClass = "CLOCK";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-byte-1-1";
							}
							else if (strcmp(T_Mode, "Minute") == 0 && Value < 0 && Value > 60) {
								CClass = "CLOCK";
								CDevice_ID = DeviceID;
								//string CAction = Action;
								device_command = CDevice_ID + "-" + CClass + "-user-byte-1-2";
							}
							else {
								Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
								Devices_status["help"] = "This Device not support on this action!!!!!";
								goto event_status_zwave;
							}
							MyValue *device_action = MyNode::lookup(string(device_command));
							if (device_action != NULL) {
								string arg = T_Value;
								if (!Manager::Get()->SetValue(device_action->getId(), arg)) {
									fprintf(stderr, "SetValue string failed type=%s\n", valueTypeStr(device_action->getId().GetType()));
									//Manager::Get()->RequestNodeDynamic(homeId, nodeID);
									//Sleep(1);
								}
							}
							else {
								//fprintf(stderr, "Can't find ValueID for %s\n", device_action);
							}
							Devices_status["status_code"] = MHD_HTTP_OK;
							Devices_status["mode"] = T_Mode;
							string target;
							Manager::Get()->RefreshNodeInfo(homeId, nodeID);
							//Sleep(1);
						}
						else {
							Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
							Devices_status["help"] = "Please check your mode selection!!!!!";
						}
					}
					else {
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
						Devices_status["help"] = "This Device not available please check your device status!!!!!";
					}
				}
				else {
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = "Please check your DeviceID!!!!!";
				}
			}
			else {
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = "Please check your API Request!!!!!";
			}
		}
		else if (strcmp(Command, "gateway_threat") == 0) {
			struct Driver::DriverData data;
			int iZwave, jZwave;
			int cnt;
			char str[16], str_nodes[16];
			//iZwave = nodeID;

			Manager::Get()->GetDriverStatistics(homeId, &data);

			Devices_status_properties["Information"]["Threat"] = data.m_ACKWaiting;
			Devices_status_properties["Information"]["Read_Aborts"] = data.m_readAborts;
			Devices_status_properties["Information"]["Bad_Checksum"] = data.m_badChecksum;
			Devices_status_properties["Information"]["Can"] = data.m_CANCnt;
			Devices_status_properties["Information"]["NACK"] = data.m_NAKCnt;
			Devices_status_properties["Information"]["Out_offrame"] = data.m_OOFCnt;

			Devices_status_properties["Information"]["SOF"] = data.m_SOFCnt;
			Devices_status_properties["Information"]["Total_Reads"] = data.m_readCnt;
			Devices_status_properties["Information"]["Total_Writes"] = data.m_writeCnt;
			Devices_status_properties["Information"]["Ack"] = data.m_ACKCnt;
			Devices_status_properties["Information"]["Broadcast_Received"] = data.m_broadcastReadCnt;
			Devices_status_properties["Information"]["Broadcast_Transmitted"] = data.m_broadcastWriteCnt;

			Devices_status_properties["Information"]["Dropped"] = data.m_dropped;
			Devices_status_properties["Information"]["Retries"] = data.m_retries;
			Devices_status_properties["Information"]["Callbacks"] = data.m_callbacks;
			Devices_status_properties["Information"]["BadRoutes"] = data.m_badroutes;
			Devices_status_properties["Information"]["No_Ack"] = data.m_noack;
			Devices_status_properties["Information"]["NetworkBusy"] = data.m_netbusy;
			Devices_status_properties["Information"]["NotIdle"] = data.m_notidle;
			Devices_status_properties["Information"]["NotDelivery"] = data.m_nondelivery;
			Devices_status_properties["Information"]["RoutesBusy"] = data.m_routedbusy;
			cnt = MyNode::getNodeCount();
			iZwave = 0;
			jZwave = 1;
			while (jZwave <= cnt && iZwave < MAX_NODES) {
				snprintf(str_nodes, sizeof(str_nodes), "%d", iZwave);
				if (nodes[iZwave] != NULL) {
					struct Node::NodeData ndata;
					Manager::Get()->GetNodeStatistics(homeId, iZwave, &ndata);
					snprintf(str, sizeof(str), "%d", iZwave);
					Devices_status_properties["Information"]["Node_Sentmessages"] = ndata.m_sentCnt;
					Devices_status_properties["Information"]["Node_Sentfailed"] = ndata.m_sentFailed;
					Devices_status_properties["Information"]["Node_Retries"] = ndata.m_retries;
					Devices_status_properties["Information"]["Node_Receivedmessages"] = ndata.m_receivedCnt;
					Devices_status_properties["Information"]["Node_ReceivedDuplicates"] = ndata.m_receivedDups;
					Devices_status_properties["Information"]["Node_ReceivedUnsolicited"] = ndata.m_receivedUnsolicited;
					Devices_status_properties["Information"]["Node_LastSentTime"] = ndata.m_sentTS.substr(5).c_str();
					Devices_status_properties["Information"]["Node_LastReceived"] = ndata.m_receivedTS.substr(5).c_str();
					Devices_status_properties["Information"]["Node_LastRTT"] = ndata.m_averageRequestRTT;
					Devices_status_properties["Information"]["Node_AverageRTT"] = ndata.m_averageRequestRTT;
					Devices_status_properties["Information"]["Node_LastRTTResponse"] = ndata.m_averageResponseRTT;
					Devices_status_properties["Information"]["Node_AverageRTTResponse"] = ndata.m_averageResponseRTT;
					Devices_status_properties["Information"]["Quality"] = ndata.m_quality;
					jZwave++;
					Devices_status["status_code"] = MHD_HTTP_OK;
					event_status[str_nodes] = Devices_status_properties;
					Devices_status_properties.clear();
				}
				iZwave++;
			}
		}
		else if (strcmp(Command, "devicedetails") == 0) {
			if (DeviceID != NULL && DeviceID[0] != '\0') {
				if (strcmp(DeviceID, "all") == 0) {
					goto GetDetailsAll;
				}
				nodeID = atoi(DeviceID);
				i = nodeID;
				nodeID_new = nodeID;
				if (nodeID < MAX_NODES) {
					if (nodes[i] != NULL) {
						goto GetDetails;
					}
					else {
						string str_error = "This Device ID not available, Please check your Device ID!";
						Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
						Devices_status["help"] = str_error.c_str();
						goto event_status_zwave;
					}
				}
				else {
					string str_error = "Please check your Device ID!";
					Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
					Devices_status["help"] = str_error.c_str();
					goto event_status_zwave;
				}
			}
			else {
				string str_error = "Oh Something went Wrong please check your API!!!!!";
				Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
				Devices_status["help"] = str_error.c_str();
				goto event_status_zwave;
			}

			//pthread_mutex_lock(&nlock);
		GetDetailsAll:
			while (z)
			{
				i = 0;
				j = 1;
				while (j <= MyNode::getNodeCount() && i < MAX_NODES)
				{
					if (nodes[i] != NULL)
					{
					GetDetails:
						snprintf(str_nodes, sizeof(str_nodes), "%d", i);
						bool listening;
						bool flirs;
						bool zwaveplus;
						Devices_status_properties["Information"]["nodeid"] = i;
						zwaveplus = Manager::Get()->IsNodeZWavePlus(homeId, i);
						if (i == 1 && nodes[i] != NULL) {
							char str_homeID[16];
							if (homeId != 0L) {
								snprintf(str_homeID, sizeof(str_homeID), "%08x", homeId);
								Devices_status_properties["Information"]["homeid"] = str_homeID;
								Devices_status_properties["Information"]["version"] = Manager::Get()->GetLibraryVersion(homeId);
							}
							else
								str_homeID[0] = '\0';
						}
						if (zwaveplus)
						{
							string value = Manager::Get()->GetNodePlusTypeString(homeId, i);
							value += " " + Manager::Get()->GetNodeRoleString(homeId, i);
							Devices_status_properties["Information"]["basictype"] = value.c_str();
							Devices_status_properties["Information"]["generictype"] = Manager::Get()->GetNodeDeviceTypeString(homeId, i).c_str();
							Devices_status_properties["Information"]["things_type"] = ThingType(Manager::Get()->GetNodeProductName(homeId, i), i);


						}
						else
						{
							Devices_status_properties["Information"]["basictype"] = nodeBasicStr(Manager::Get()->GetNodeBasic(homeId, i));
							Devices_status_properties["Information"]["generictype"] = Manager::Get()->GetNodeType(homeId, i).c_str();
						}
						Devices_status_properties["Information"]["manufacturer"] = Manager::Get()->GetNodeManufacturerName(homeId, i).c_str();
						Devices_status_properties["Information"]["name"] = Manager::Get()->GetNodeName(homeId, i).c_str();
						Devices_status_properties["Information"]["location"] = Manager::Get()->GetNodeLocation(homeId, i).c_str();
						Devices_status_properties["Information"]["product"] = Manager::Get()->GetNodeProductName(homeId, i).c_str();
						Devices_status_properties["Information"]["things_type"] = ThingType(Manager::Get()->GetNodeProductName(homeId, i), i);
						Devices_status_properties["Information"]["listening"] = Manager::Get()->IsNodeListeningDevice(homeId, i) ? "true" : "false";
						Devices_status_properties["Information"]["frequent"] = Manager::Get()->IsNodeFrequentListeningDevice(homeId, i) ? "true" : "false";
						Devices_status_properties["Information"]["beam"] = Manager::Get()->IsNodeBeamingDevice(homeId, i) ? "true" : "false";
						Devices_status_properties["Information"]["routing"] = Manager::Get()->IsNodeRoutingDevice(homeId, i) ? "true" : "false";
						Devices_status_properties["Information"]["security"] = Manager::Get()->IsNodeSecurityDevice(homeId, i) ? "true" : "false";
						Devices_status_properties["Information"]["time"] = nodes[i]->getTime();
						Devices_status_properties["Information"]["node_count"] = MyNode::getNodeCount();
						listening = Manager::Get()->IsNodeListeningDevice(homeId, i);
						flirs = Manager::Get()->IsNodeFrequentListeningDevice(homeId, i);
#if 0
						fprintf(stderr, "i=%d failed=%d\n", i, Manager::Get()->IsNodeFailed(homeId, i));
						fprintf(stderr, "i=%d awake=%d\n", i, Manager::Get()->IsNodeAwake(homeId, i));
						fprintf(stderr, "i=%d state=%s\n", i, Manager::Get()->GetNodeQueryStage(homeId, i).c_str());
						fprintf(stderr, "i=%d listening=%d flirs=%d\n", i, listening, flirs);
#endif
						if (Manager::Get()->IsNodeFailed(homeId, i))
						{
							Devices_status_properties["Information"]["status"] = "Dead";
						}
						else
						{
							string s = Manager::Get()->GetNodeQueryStage(homeId, i);
							if (s == "Complete")
							{
								if (i != nodeId && !listening && !flirs)
								{
									if (i == 1) {
										Devices_status_properties["Information"]["status"] = "Ready";
									}
									else {
										Devices_status_properties["Information"]["status"] = Manager::Get()->IsNodeAwake(homeId, i) ? "Awake" : "Sleeping";
									}
								}
								else
								{
									if ( i == 1) {
										Devices_status_properties["Information"]["status"] = "Ready";
									}
									else {
										Devices_status_properties["Information"]["status"] = "Ready";
									}
								}

							}
							else
							{
								if (i != nodeId && !listening && !flirs)
								{
									Devices_status_properties["Information"]["status"] = Manager::Get()->IsNodeAwake(homeId, i) ? "Awake" : "Sleeping";
								}
								else
									Devices_status_properties["Information"]["status"] = "Ready";
							}
						}
						int32 idcnt = nodes[i]->getValueCount();
						//Json::Value properties;
						for (int k = 0; k < idcnt; k++)
						{
							MyValue *vals = nodes[i]->getValue(k);
							ValueID id = vals->getId();
							string str;
							bool thermostat_mode = (strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Cooling 1") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Heating 1") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Unused 0") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Auto Changeover") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Heating Econ") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Unused 3") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Unused 4") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Unused 5") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Unused 6") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Furnace") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Dry Air") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Moist Air") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Cooling Econ") == 0 ||
								strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Away Heating") == 0);
							if (strcmp(Manager::Get()->GetNodeType(homeId, i).c_str(), "Routing Alarm Sensor") == 0 || strcmp(Manager::Get()->GetNodeType(homeId, i).c_str(), "Routing Smoke Sensor") == 0)
							{
								if (strcmp(cclassStr(id.GetCommandClassId()), "SENSOR ALARM") == 0)
								{
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["CommandClass"] = cclassStr(id.GetCommandClassId());
									if (Manager::Get()->GetValueAsString(id, &str))
									{
										Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["value"] = str.c_str();
									}
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["type"] = valueTypeStr(id.GetType());
								}
							}
							if (strcmp(Manager::Get()->GetNodeType(homeId, i).c_str(), "Home Security Sensor") == 0)
							{
								if ((strcmp(cclassStr(id.GetCommandClassId()), "ALARM") == 0) && (strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Burglar") == 0))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["CommandClass"] = cclassStr(id.GetCommandClassId());
									if (Manager::Get()->GetValueAsString(id, &str))
									{
										Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["value"] = str.c_str();
									}
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["type"] = valueTypeStr(id.GetType());
								}
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "SWITCH BINARY") == 0 || strcmp(cclassStr(id.GetCommandClassId()), "SENSOR BINARY") == 0)
							{
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["CommandClass"] = cclassStr(id.GetCommandClassId());
								if (Manager::Get()->GetValueAsString(id, &str))
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["value"] = str.c_str();
								//else
								if (id.GetType() == ValueID::ValueType_Decimal)
								{
									uint8 precision;
									if (Manager::Get()->GetValueFloatPrecision(id, &precision))
										fprintf(stderr, "node = %d id = %d value = %s precision = %d\n", i, j, str.c_str(), precision);
								}
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["type"] = valueTypeStr(id.GetType());
								//break;
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "BATTERY") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["battery"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["battery"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["battery"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["battery"]["type"] = valueTypeStr(id.GetType());
							}

							if (strcmp(cclassStr(id.GetCommandClassId()), "DOOR LOCK") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Locked") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["CommandClass"] = cclassStr(id.GetCommandClassId());
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "SWITCH MULTILEVEL") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Level") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["CommandClass"] = cclassStr(id.GetCommandClassId());
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["A-GenericValues"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "SENSOR MULTILEVEL") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Temperature") == 0)
							{
								if (strstr(Manager::Get()->GetNodeDeviceTypeString(homeId, i).c_str(), "Thermostat") != NULL ||
									strstr(Manager::Get()->GetNodeType(homeId, i).c_str(), "Thermostat") != NULL) {
									Sensors = 0;
								}else {
									Sensors++;
								}
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["temperature"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["temperature"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["temperature"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["temperature"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "SENSOR MULTILEVEL") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Relative Humidity") == 0)
							{
								if (strstr(Manager::Get()->GetNodeDeviceTypeString(homeId, i).c_str(), "Thermostat") != NULL ||
									strstr(Manager::Get()->GetNodeType(homeId, i).c_str(), "Thermostat") != NULL) {
									Sensors = 0;
								}else {
									Sensors++;
								}
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["humidity"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["humidity"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["humidity"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["humidity"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "SENSOR MULTILEVEL") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Ultraviolet") == 0)
							{
								Sensors++;
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["ultraviolet"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["ultraviolet"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["ultraviolet"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["ultraviolet"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "SENSOR MULTILEVEL") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Luminance") == 0)
							{
								Sensors++;
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["luminance"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["luminance"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["luminance"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["luminance"]["type"] = valueTypeStr(id.GetType());
							}
							if ((strcmp(cclassStr(id.GetCommandClassId()), "ALARM") == 0) && (strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Burglar") == 0))
							{
								Sensors++;
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									if (strcmp(str.c_str(), "3") == 0)
										Devices_status_properties["Information"]["zWaveproperties"]["vibration"]["value"] = str.c_str();
									else
										Devices_status_properties["Information"]["zWaveproperties"]["vibration"]["value"] = "0";
								}
								Devices_status_properties["Information"]["zWaveproperties"]["vibration"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["vibration"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["vibration"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "COLOR") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Color") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Color"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Color"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Color"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Color"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "METER") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Energy") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Energy"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Energy"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Energy"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Energy"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "METER") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Current") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Current"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Current"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Current"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Current"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "METER") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Previous Reading") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Previous Reading"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Previous Reading"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Previous Reading"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Previous Reading"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "METER") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Power") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Power"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Power"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Power"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Power"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "METER") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Voltage") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Voltage"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Voltage"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Voltage"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Voltage"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "THERMOSTAT MODE") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Mode"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Mode"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Mode"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Mode"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "THERMOSTAT OPERATING STATE") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Operating State"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Operating State"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Operating State"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Operating State"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "THERMOSTAT SETPOINT") == 0) {
								if (thermostat_mode) {
									if (Manager::Get()->GetValueAsString(id, &str))
									{
										Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Setpoint"][Manager::Get()->GetValueLabel(id).c_str()]["value"] = str.c_str();
									}
									Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Setpoint"][Manager::Get()->GetValueLabel(id).c_str()]["units"] = Manager::Get()->GetValueUnits(id).c_str();
									Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Setpoint"][Manager::Get()->GetValueLabel(id).c_str()]["label"] = Manager::Get()->GetValueLabel(id).c_str();
									Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Setpoint"][Manager::Get()->GetValueLabel(id).c_str()]["type"] = valueTypeStr(id.GetType());
								}
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "THERMOSTAT FAN MODE") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan Mode"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan Mode"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan Mode"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan Mode"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "THERMOSTAT FAN STATE") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan State"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan State"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan State"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Thermostat Fan State"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "CLOCK") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Day") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Day"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Day"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Day"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Day"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "CLOCK") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Hour") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Hour"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Hour"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Hour"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Hour"]["type"] = valueTypeStr(id.GetType());
							}
							if (strcmp(cclassStr(id.GetCommandClassId()), "CLOCK") == 0 && strcmp(Manager::Get()->GetValueLabel(id).c_str(), "Minute") == 0)
							{
								if (Manager::Get()->GetValueAsString(id, &str))
								{
									Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Minute"]["value"] = str.c_str();
								}
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Minute"]["units"] = Manager::Get()->GetValueUnits(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Minute"]["label"] = Manager::Get()->GetValueLabel(id).c_str();
								Devices_status_properties["Information"]["zWaveproperties"]["Clock"]["Minute"]["type"] = valueTypeStr(id.GetType());
							}
						}
						if (Sensors != 0) {
							if (Sensors == 1) {
								Devices_status_properties["Information"]["things_type"] = "2 in 1 Sensor";
							}
							else if (Sensors == 2) {
								Devices_status_properties["Information"]["things_type"] = "3 in 1 Sensor";
							}
							else if (Sensors == 3) {
								Devices_status_properties["Information"]["things_type"] = "4 in 1 Sensor";
							}
							else if (Sensors == 4) {
								Devices_status_properties["Information"]["things_type"] = "5 in 1 Sensor";
							}
							else if (Sensors == 5) {
								Devices_status_properties["Information"]["things_type"] = "6 in 1 Sensor";
							}
							else {
								Sensors = 0;
							}
						}
						Devices_status["status_code"] = MHD_HTTP_OK;
						event_status[str_nodes] = Devices_status_properties;
						Devices_status_properties.clear();
					}
					Sensors = 0;
					i++;
					if (nodeID_new == nodeID && nodeID != 0) { break; }
				}
				z = 0;
			}
			//pthread_mutex_unlock(&nlock);
		}
		else {
			snprintf(OBox, sizeof(OBox), "%d", Event_ZWave);
			Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
			Devices_status["help"] = "Please check your actions (Add/Remove/Cancel/Refresh/Reset/Rename/SetTarget)";
			//pthread_mutex_unlock(&nlock);
		}
	}
	else {
		string str_error = "Oh Something went Wrong please check your API!!!!!";
		Devices_status["status_code"] = MHD_HTTP_BAD_REQUEST;
		Devices_status["help"] = str_error.c_str();
	}
event_status_zwave:
	event_status["Controller_Command"] = Devices_status;
event_status_zwave_end:
	if (Log != NULL && Log[0] != '\0') {
		if (strcmp(Log, "YES") == 0) {
			char log_buf[2048];
			memset(log_buf, 0, 2048);
			snprintf(log_buf, sizeof(log_buf), "%slog.json", log_path);
			file_log.open(log_buf);
			fprintf(stderr, "Log location is: %s\n", log_buf);
			file_log << styledWriter.write(event_status);
			file_log.close();
		}
	}
	snprintf(temp_buf, sizeof(temp_buf), "%sozwcp.device_action.XXXXXX", command_path);
	strncpy(fntemp, temp_buf, sizeof(fntemp));
	//fn = tempnam(fntemp, NULL);
	fn = tmpnam(fntemp);
	//fn = tempnam(fntemp, NULL);
	if (fn == NULL)
		return MHD_YES;
	strncat(fntemp, ".json", sizeof(fntemp));
	file_id.open(fn);
	file_id << styledWriter.write(event_status);
	file_id.close();

	ret = web_send_file(conn, fn, MHD_HTTP_OK, true);
	//free(fn);
	//system("del /q/f/s %TEMP%\\*.json");
	return ret;
}



/*
 * web_controller_update
 * Handle controller function feedback from library.
 */

void web_controller_update(Driver::ControllerState cs, Driver::ControllerError err, void *ct)
{
	Webserver *cp = (Webserver *)ct;
	string s;
	bool more = true;
	switch (cs) {
	case Driver::ControllerState_Normal:
		s = ": no command in progress.";
		break;
	case Driver::ControllerState_Starting:
		s = ": starting controller command.";
		break;
	case Driver::ControllerState_Cancel:
		s = ": command was cancelled.";
		more = false;
		break;
	case Driver::ControllerState_Error:
		s = ": command returned an error: ";
		more = false;
		break;
	case Driver::ControllerState_Sleeping:
		s = ": device went to sleep.";
		more = false;
		break;
	case Driver::ControllerState_Waiting:
		s = " -> Tap the Z-Wave button on the device which you want to detect by this O Box. ";
		break;
	case Driver::ControllerState_InProgress:
		s = ": communicating with the other device.";
		break;
	case Driver::ControllerState_Completed:
		s = ": command has completed successfully.";
		more = false;
		break;
	case Driver::ControllerState_Failed:
		s = ": command has failed.";
		more = false;
		break;
	case Driver::ControllerState_NodeOK:
		s = ": the node is OK.";
		more = false;
		break;
	case Driver::ControllerState_NodeFailed:
		s = ": the node has failed.";
		more = false;
		break;
	default:
		s = ": unknown response.";
		break;
	}
	if (err != Driver::ControllerError_None)
		s = s + controllerErrorStr(err);

	command_str = s;
	cp->setAdminMessage(s);
	cp->setAdminState(more);
	Manager::Get()->WriteConfig(homeId);
	pthread_mutex_lock(&glock);
	needsave = false;
	pthread_mutex_unlock(&glock);
}

/*
 * web_config_post
 * Handle the post of the updated data
 */

int web_config_post (void *cls, enum MHD_ValueKind kind, const char *key, const char *filename,
		const char *content_type, const char *transfer_encoding,
		const char *data, uint64_t off, size_t size)
{
	conninfo_t *cp = (conninfo_t *)cls;

	fprintf(stderr, "post: key=%s data=%s size=%d\n", key, data, size);
	if (strcmp(cp->conn_url, "/devpost.html") == 0) {
		if (strcmp(key, "fn") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "dev") == 0)
			cp->conn_arg2 = (void *)strdup(data);
		else if (strcmp(key, "usb") == 0)
			cp->conn_arg3 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/valuepost.html") == 0) {
		cp->conn_arg1 = (void *)strdup(key);
		cp->conn_arg2 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/buttonpost.html") == 0) {
		cp->conn_arg1 = (void *)strdup(key);
		cp->conn_arg2 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/admpost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "node") == 0)
			cp->conn_arg2 = (void *)strdup(data);
		else if (strcmp(key, "button") == 0)
			cp->conn_arg3 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/nodepost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "node") == 0)
			cp->conn_arg2 = (void *)strdup(data);
		else if (strcmp(key, "value") == 0)
			cp->conn_arg3 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/grouppost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "node") == 0)
			cp->conn_arg2 = (void *)strdup(data);
		else if (strcmp(key, "num") == 0)
			cp->conn_arg3 = (void *)strdup(data);
		else if (strcmp(key, "groups") == 0)
			cp->conn_arg4 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/pollpost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "node") == 0)
			cp->conn_arg2 = (void *)strdup(data);
		else if (strcmp(key, "ids") == 0)
			cp->conn_arg3 = (void *)strdup(data);
		else if (strcmp(key, "poll") == 0)
			cp->conn_arg4 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/savepost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/scenepost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "id") == 0)
			cp->conn_arg2 = (void *)strdup(data);
		else if (strcmp(key, "vid") == 0)
			cp->conn_arg3 = (void *)strdup(data);
		else if (strcmp(key, "label") == 0)
			cp->conn_arg3 = (void *)strdup(data);
		else if (strcmp(key, "value") == 0)
			cp->conn_arg4 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/topopost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/statpost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/thpost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		if (strcmp(key, "num") == 0)
			cp->conn_arg2 = (void *)strdup(data);
		if (strcmp(key, "cnt") == 0)
			cp->conn_arg3 = (void *)strdup(data);
		if (strcmp(key, "healrrs") == 0)
			cp->conn_arg3 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/confparmpost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "node") == 0)
			cp->conn_arg2 = (void *)strdup(data);
	} else if (strcmp(cp->conn_url, "/refreshpost.html") == 0) {
		if (strcmp(key, "fun") == 0)
			cp->conn_arg1 = (void *)strdup(data);
		else if (strcmp(key, "node") == 0)
			cp->conn_arg2 = (void *)strdup(data);
	}
	return MHD_YES;
}

/*
 * Process web requests
 */
int Webserver::HandlerEP (void *cls, struct MHD_Connection *conn, const char *url,
		const char *method, const char *version, const char *up_data,
		size_t *up_data_size, void **ptr)
{
	Webserver *ws = (Webserver *)cls;

	return ws->Handler(conn, url, method, version, up_data, up_data_size, ptr);
}

int Webserver::Handler (struct MHD_Connection *conn, const char *url,
		const char *method, const char *version, const char *up_data,
		size_t *up_data_size, void **ptr)
{
	int ret;
	conninfo_t *cp;

	if (debug)
		fprintf(stderr, "%x: %s: \"%s\" conn=%x size=%d *ptr=%x\n", pthread_self(), method, url, conn, *up_data_size, *ptr);
	if (*ptr == NULL) {	/* do never respond on first call */
		cp = (conninfo_t *)malloc(sizeof(conninfo_t));
		if (cp == NULL)
			return MHD_NO;
		cp->conn_url = url;
		cp->conn_arg1 = NULL;
		cp->conn_arg2 = NULL;
		cp->conn_arg3 = NULL;
		cp->conn_arg4 = NULL;
		cp->conn_res = NULL;
		if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
			cp->conn_pp = MHD_create_post_processor(conn, 1024, web_config_post, (void *)cp);
			if (cp->conn_pp == NULL) {
				free(cp);
				return MHD_NO;
			}
			cp->conn_type = CON_POST;
		} else if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
			cp->conn_type = CON_GET;
		} else {
			free(cp);				
			return MHD_NO;
		}
		*ptr = (void *)cp;
		return MHD_YES;
	}
	if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
		if (strcmp(url, "/devicepollingevery3seconds") == 0 ) {
			ret = SendPollResponse(conn);
		} else if (strcmp(url, "/user_request") == 0) {
			ret = SendDeviceID_userdata(conn);
		} else
			ret = web_send_data(conn, UNKNOWN, MHD_HTTP_NOT_FOUND, false, false, NULL); // no free, no copy
		return ret;
	} else if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
		// Nothing will do;
		return MHD_NO;
	} else
		return MHD_NO;
}

/*
 * web_free
 * Free up any allocated data after connection closed
 */

void Webserver::FreeEP (void *cls, struct MHD_Connection *conn,
		void **ptr, enum MHD_RequestTerminationCode code)
{
	Webserver *ws = (Webserver *)cls;

	ws->Free(conn, ptr, code);
}

void Webserver::Free (struct MHD_Connection *conn, void **ptr, enum MHD_RequestTerminationCode code)
{
	conninfo_t *cp = (conninfo_t *)*ptr;

	if (cp != NULL) {
		if (cp->conn_arg1 != NULL)
			free(cp->conn_arg1);
		if (cp->conn_arg2 != NULL)
			free(cp->conn_arg2);
		if (cp->conn_arg3 != NULL)
			free(cp->conn_arg3);
		if (cp->conn_arg4 != NULL)
			free(cp->conn_arg4);
		if (cp->conn_type == CON_POST) {
			MHD_destroy_post_processor(cp->conn_pp);
		}
		free(cp);
		*ptr = NULL;
	}
}

/*
 * Constructor
 * Start up the web server
 */

Webserver::Webserver (int const wport, char *serial_port) : sortcol(COL_NODE), logbytes(0), adminstate(false)
{
	fprintf(stderr, "webserver starting port %d\n", wport);
	port = wport;
	//strcpy(devname, /*(char *)cp->conn_arg2*/);
	Manager::Get()->AddDriver(serial_port);
	MyNode::setAllChanged(true);
	wdata = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG, port,
			NULL, NULL, &Webserver::HandlerEP, this,
			MHD_OPTION_NOTIFY_COMPLETED, Webserver::FreeEP, this, MHD_OPTION_END);
	
	if (wdata != NULL) {
		ready = true;
	}
}

/*
 * Destructor
 * Stop the web server
 */

Webserver::~Webserver ()
{
	if (wdata != NULL) {
		MHD_stop_daemon((MHD_Daemon *)wdata);
		wdata = NULL;
		ready = false;
	}
}
