//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Modification history
//
// 2018-04-05 MJW Many fixups for networking but still not working for OpenTTP
// 2018-09-03 ELM More fixups for menu structure and reading GPSDO status

#include "Debug.h"

#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timex.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <utime.h>
#include <ifaddrs.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>

#include <algorithm>
#include <iostream>
#include <stack>
#include <fstream>
#include <sstream>
#include <string>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "configurator.h"

#include "Button.h"
#include "ConfirmationDialog.h"
#include "ContrastWidget.h"
#include "Dialog.h"
#include "IntensityWidget.h"
#include "IPWidget.h"
#include "NumberWidget.h"
#include "KeyEvent.h"
#include "Label.h"
#include "LCDMonitor.h"
#include "Menu.h"
#include "MessageBox.h"
#include "SliderWidget.h"
#include "Version.h"
#include "Widget.h"
#include "WidgetCallback.h"
#include "Wizard.h"

#define LCDMONITOR_VERSION "3.1.1"

#define BAUD 115200
#define PORT "/dev/lcd"
#define DEFAULT_LOG_FILE        "/usr/local/log/lcdmonitor.log"
#define DEFAULT_LOCK_FILE       "/usr/local/log/lcdmonitor.lock"
#define DEFAULT_CONFIG          "/usr/local/etc/lcdmonitor.conf"

#define PRETTIFIER "*********************************************"

#define BOOT_GRACE_PERIOD 300 // in seconds
#define MAX_FILE_AGE 300

using namespace std;
using namespace::boost;

bool LCDMonitor::timeout=false;
extern LCDMonitor *app;
bool showHealth = true;
int statusline = 0;

extern ostream *debugStream;
extern string   debugFileName;
extern ofstream debugLog;
extern int verbosity;
extern bool shortDebugMessage;

LCDMonitor::LCDMonitor(int argc,char **argv)
{
	verbosity=TRACE;
	configFile = DEFAULT_CONFIG;
	
	int c;
	while ((c=getopt(argc,argv,"c:hvd:")) != EOF)
	{
		switch(c)
  	{
			case 'c':
				configFile = optarg;
				break;
			case 'h':showHelp(); exit(EXIT_SUCCESS);
			case 'v':showVersion();exit(EXIT_SUCCESS);
			case 'd':
			{
				string dbgout = optarg;
				if ((string::npos != dbgout.find("stderr"))){
					debugStream = & std::cerr;
				}
				else{
					debugFileName = dbgout;
					debugLog.open(debugFileName.c_str(),ios_base::app);
					if (!debugLog.is_open()){
						cerr << "Unable to open " << dbgout << endl;
						exit(EXIT_FAILURE);
					}
					debugStream = & debugLog;
				}
				break;
			}
		}
	}

	init();

	makeMenu();

}

LCDMonitor::~LCDMonitor()
{
	clearDisplay();
	statusLEDsOff();
	Uninit_Serial();
	log("Shutdown");
	unlink(lockFile.c_str());
}

void LCDMonitor::touchLock()
{
	// the lock is touched periodically to signal that
	// the process is still alive
	utime(lockFile.c_str(),0);
}

void LCDMonitor::showAlarms()
{
	clearDisplay();

	MessageBox *mb = new MessageBox(" "," "," "," ");

	unsigned int nalarms=alarms.size();
	if (nalarms > 4) nalarms=4;

	if (nalarms == 0){
		mb->setLine(1,"   No alarms");
	}
	else{
		for (unsigned int i=0;i<nalarms;i++)
			mb->setLine(i,alarms[i]);
	}

	execDialog(mb);
	delete mb;
}

void LCDMonitor::showSysInfo()
{
	clearDisplay();

	MessageBox *mb = new MessageBox(" "," "," "," ");

	int nline=0;

	ifstream fin(sysInfoConf.c_str());
	if (!fin.good())
		mb->setLine(1,"File not found");
	else{
		string tmp;

		while (!fin.eof()){
			getline(fin,tmp);
			if (fin.eof())
				break;
			if (fin.fail()){
				mb->setLine(0," ");mb->setLine(2," ");mb->setLine(3," ");
				mb->setLine(1,"Bad sysinfo file");
				break;
			}
			mb->setLine(nline,tmp);
			nline++;
		}
	}

	execDialog(mb);
	delete mb;
}

void LCDMonitor::showIP()
{
	std::string lan1ip, lan2ip,lan1mac,lan2mac;
	getNetworkInterfaces(lan1ip,lan1mac,lan2ip,lan2mac);
	clearDisplay();

	MessageBox *mb = new MessageBox(" "," "," "," ");

	if(lan1ip != "") {
		mb->setLine(0,"LAN1: " + lan1ip);
		mb->setLine(1," " + lan1mac);
	}
	else{
		mb->setLine(0,"LAN1: not set" );
		if (lan1mac != ""){
			mb->setLine(1," " + lan1mac);
		}
	}
	
	if (LANIFname[1] != ""){
		if(lan2ip != "") {
			mb->setLine(2,"LAN2: " + lan2ip);
			mb->setLine(3," " + lan2mac);
		}
		else{
			mb->setLine(2,"LAN2: not set" );
			if (lan2mac != ""){
				mb->setLine(3," " + lan2mac);
			}
		}
	}
	
	execDialog(mb);
	delete mb;
}

// This is needed because if we are using DHCP, we don't know what the assigned address is
void LCDMonitor::getNetworkInterfaces(std::string &lan0ip, std::string &lan0mac,std::string &lan1ip,std::string &lan1mac)
{
	struct ifaddrs * ifAddrStruct=NULL;
	struct ifaddrs * ifa=NULL;
	void * tmpAddrPtr=NULL;
	char mac[32];
	if (-1 == getifaddrs(&ifAddrStruct)){
		log("Failed to query network interfaces"); 
		return;
	}
	
	// systemd assigns names to Ethernet interfaces (en) like ..
	//eno: Names containing the index numbers provided by firmware/BIOS for on-board devices, example: eno1 (eno = Onboard).
	//ens: Names containing the PCI Express hotplug slot numbers provided by the firmware/BIOS, example: ens1 (ens = Slot).
	//enp: Names containing the physical/geographical location of the hardware's port, example: enp2s0 (enp = Position).
	//enx: Names containing the MAC address of the interface (example: enx78e7d1ea46da).
	//eth: Classic unpredictable kernel-native ethX naming (example: eth0).

	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr) {
			
			continue;
		}
		
		if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
			// is a valid IP4 Address?
			tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			char addressBuffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			DBGMSG(debugStream,TRACE,ifa->ifa_name);
			if (LANIFname[0] == ifa->ifa_name){
				DBGMSG(debugStream,TRACE,"found ipv4 " << ifa->ifa_name);
				lan0ip = addressBuffer;
			}
			else if (LANIFname[1] == ifa->ifa_name){
				DBGMSG(debugStream,TRACE,"found ipv4 " << ifa->ifa_name);
				lan1ip= addressBuffer;
				
			}
		}
		
		if (ifa->ifa_addr->sa_family == AF_PACKET) { // check for link layer	
 			DBGMSG(debugStream,TRACE,ifa->ifa_name);
 			if (LANIFname[0] == ifa->ifa_name){
 				DBGMSG(debugStream,TRACE,"found link layer " << ifa->ifa_name);
 				struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
				lan0mac = "";
 				for (int i = 0; i < s->sll_halen; i++) {
 					sprintf(mac,"%02x%c", s->sll_addr[i], (i + 1 != s->sll_halen) ? ':' : ' '); 
					lan0mac += mac;
 				}
		
 				DBGMSG(debugStream,TRACE,lan0mac);
 			}
 			else if (LANIFname[1] == ifa->ifa_name){
				DBGMSG(debugStream,TRACE,"found link layer " << ifa->ifa_name);
 				struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
 				for (int i = 0; i < s->sll_halen; i++) {
 					sprintf(mac,"%02x%c", s->sll_addr[i], (i + 1 != s->sll_halen) ? ':' : ' ');
 				}
 				lan1mac = mac;
 				DBGMSG(debugStream,TRACE,lan1mac);
 			}
 		}
		
	}
	if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
}

void LCDMonitor::networkConfigDHCP(int ifID)
{

	if (customNetCfg){ // decline to edit
		MessageBox *mb = new MessageBox("Unable to edit.","Custom configuration","flagged"," ");
		execDialog(mb);
		delete mb;
		return;
	}
	
	int oldAddressAssignment;
	if (ifID == 0)
		oldAddressAssignment = addressAssignmentLAN[ifID];
	
	if (ifID == 1)
		oldAddressAssignment = addressAssignmentLAN[ifID];
	
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm DHCP");
	bool ret = execDialog(dlg);
	std::string lastError="No error";
	
	if (ret){
				
		nets.at(ifID)->DHCP = true;
		writeNetPlanConfig(ifID);
		if (restartNetworking()){
			addressAssignmentLAN[ifID] = DHCP;
		}
	}

	{
	// Cleanup time
	delete dlg;

	int newNetworkProtocol=DHCP;
	if (!ret && oldAddressAssignment != DHCP)
		newNetworkProtocol = Static;
	
	// Update the menu FIXEM
	MenuItem *mi = LAN0M->itemAt(midDHCP0);
	mi->setChecked(newNetworkProtocol==DHCP);
	mi = LAN0M->itemAt(midStaticIP40);
	mi->setChecked(newNetworkProtocol==Static);

	return;
	}

	DIE:
		delete dlg;
		DBGMSG(debugStream,TRACE, "last error: " << lastError);
		clearDisplay();
		updateLine(1,"Reconfig failed !");
		sleep(2);
}

void LCDMonitor::networkConfigStaticIP4(int ifID)
{
	int oldAddressAssignment = addressAssignmentLAN[ifID];
	
	clearDisplay();
	
	if (customNetCfg){ // decline to edit
		MessageBox *mb = new MessageBox("Unable to edit.","Custom configuration","flagged"," ");
		execDialog(mb);
		delete mb;
		return;
	}
	
	Wizard *dlg = new Wizard();

	// It is assumed that the first entry in the list is the required interface
	
	// Set defaults
	std::string ipv4addr  = "192.168.1.129";
	std::string ipv4nm    = "255.255.255.0";
	std::string ipv4gw    = "192.168.1.1" ;
	std::string ipv4ns1   = "0.0.0.0" ;
	std::string ipv4ns2   = "0.0.0.0" ;
	
	// FIXME just some debugging
	for (unsigned int l=0;l<nets.size();l++){
		DBGMSG(debugStream,TRACE,"NET " << l);
		DBGMSG(debugStream,TRACE,nets.at(l)->name);
		DBGMSG(debugStream,TRACE,nets.at(l)->address << "/" << nets.at(l)->netmask);
		for (unsigned int n=0;n<nets.at(l)->nameservers.size();n++){
			DBGMSG(debugStream,TRACE,nets.at(l)->nameservers.at(n));
		}
		DBGMSG(debugStream,TRACE,nets.at(l)->gateway);
	}
	
	if (!nets.at(ifID)->DHCP){
		ipv4addr  = nets.at(ifID)->address;
		ipv4nm    = nets.at(ifID)->netmask;
		ipv4gw    = nets.at(ifID)->gateway;
		ipv4ns1   = nets.at(ifID)->nameservers.at(0);
		if (nets.at(ifID)->nameservers.size() == 2){
			ipv4ns2   = nets.at(ifID)->nameservers.at(1);
		}
		else{
			nets.at(ifID)->nameservers.push_back("0.0.0.0");
		}
	}
	
	Widget *w = dlg->addPage("IP address");
	w->setGeometry(0,0,20,4);
	IPWidget *ipw = new IPWidget(ipv4addr,IPWidget::IPV4,w);
	ipw->setGeometry(0,1,15,1);
	ipw->setFocusWidget(true);

	w = dlg->addPage("Net mask");
	w->setGeometry(0,0,20,4);
	IPWidget *nmw = new IPWidget(ipv4nm,IPWidget::IPV4,w);
	nmw->setGeometry(0,1,15,1);
	nmw->setFocusWidget(true);

	w = dlg->addPage("Gateway");
	w->setGeometry(0,0,20,4);
	IPWidget *gww = new IPWidget(ipv4gw,IPWidget::IPV4,w);
	gww->setGeometry(0,1,15,1);
	gww->setFocusWidget(true);

	w = dlg->addPage("Nameserver 1");
	w->setGeometry(0,0,20,4);
	IPWidget *ns1w = new IPWidget(ipv4ns1,IPWidget::IPV4,w);
	ns1w->setGeometry(0,1,15,1);
	ns1w->setFocusWidget(true);

	w = dlg->addPage("Nameserver 2");
	w->setGeometry(0,0,20,4);
	IPWidget *ns2w = new IPWidget(ipv4ns2,IPWidget::IPV4,w);
	ns2w->setGeometry(0,1,15,1);
	ns2w->setFocusWidget(true);
	
	bool ret = execDialog(dlg);
	
	if (ret){
		nets.at(ifID)->address =	ipw->ipAddress();
		nets.at(ifID)->netmask =  nmw->ipAddress();
		nets.at(ifID)->gateway =  gww->ipAddress();
		nets.at(ifID)->nameservers.at(0) = ns1w->ipAddress();
		nets.at(ifID)->nameservers.at(1) = ns2w->ipAddress();
		nets.at(ifID)->DHCP = false;

		if (!writeNetPlanConfig(ifID))
			goto FAIL;
		{
		clearDisplay();
		updateLine(1,"Please wait");			
		sleep(3);
		
		if (restartNetworking())
			addressAssignmentLAN[ifID] = Static;
		}
	} // if dialog accepted
	
  {
	delete dlg;
	
	int newNetworkProtocol=Static;
	if (!ret && oldAddressAssignment != Static)
		newNetworkProtocol = DHCP;
	
	if (ifID == 0){
		MenuItem *mi = LAN0M->itemAt(midDHCP0); // FIXME
		mi->setChecked(newNetworkProtocol==DHCP);
		mi = LAN0M->itemAt(midStaticIP40);
		mi->setChecked(newNetworkProtocol==Static);
	}
	else if (ifID == 1){
		MenuItem *mi = LAN1M->itemAt(midDHCP1); // FIXME
		mi->setChecked(newNetworkProtocol==DHCP);
		mi = LAN1M->itemAt(midStaticIP41);
		mi->setChecked(newNetworkProtocol==Static);
	}
	return;
	}
	
	FAIL:
		delete dlg;
		clearDisplay();
		updateLine(1,"Reconfig failed !");
		sleep(2);
}

void LCDMonitor::readNetPlanConfig(){
std::vector<std::string> cfg;
	boost::smatch matches;
	
	//runCommand("netplan get",cfg); // we could just read the file ...
	// and indeed we will
	
	// Read the file into a vector of strings
	// That way, if we ever want to go back to using "netplan get" then it's easy
	ifstream fin(netCfg.c_str());
	if (!fin.good()){
		//
	}	
	else{
		string tmp;

		while (!fin.eof()){
			getline(fin,tmp);
			boost::regex re("^#\\s*\\CUSTOM\\s+CONFIGURATION\\s*=\\s*TRUE"); // single IPv4 address ONLY
			if (boost::regex_search(tmp,matches,re)){
				customNetCfg = true; // but keep on reading
			}
			cfg.push_back(tmp);
		}
	}
	
	
	// Initial set up is typically DHCP on all interfaces
	addressAssignmentLAN[0] = DHCP;
	
	unsigned int l = 0;
  
	// This will get called each time the network information is edited
	// in case information has been manually edited
	for (unsigned int i=0;i<nets.size();i++){ // first time through this will be empty
		nets.at(i)->nameservers.clear();
		delete nets.at(i);
	nets.clear();
	}
	unsigned int ifcnt = 0;
	unsigned int state=0x0;
	NetworkInterface *net=NULL;
	while (l<cfg.size()){
		
		std::string str = cfg.at(l);
		boost::trim(str);
		if (str.empty()){ // skip empty lines
			l++;
			continue;
		}
		if (str.at(0)=='#'){ // skip comments
			l++;
			continue;
		}
		str = cfg.at(l);
		boost::trim_right(str);
		// determine the indent level
		unsigned int c=0;
		int cnt=0;
		while (c<str.size()){
			if (str[c] != ' '){
				break;
			}
			else{
				cnt+=1;
				c+=1;
			}
		}
		
		cnt = cnt/2;
			
		// the parser 
		boost::trim_left(str);
		switch (cnt){
			case 0: // 'network is defined at this level - nothing to do
				state = 0x0;
				break;
			case 1: // looking for 'ethernets','version' and 'renderer'
			{
				if (str == "ethernets:"){
					state = 0x01;
				}
				else if (str.find("version:",0)==0){
					int sep = str.find(":");
					NPversion = str.substr(sep+1);
					boost::trim(NPversion);
				}
				else if (str.find("renderer:",0)==0){
					int sep = str.find(":");
					NPrenderer = str.substr(sep+1);
					boost::trim(NPrenderer);
				}
				break;
			}
			case 2: // interface names at this level
				if (state & 0x01){
					ifcnt = ifcnt + 1;
					str.resize(str.size()-1); // chop off trailing colon
					net = new NetworkInterface();
					net->name = str;
					net->DHCP = true;
					nets.push_back(net);
					state = 0x01 | 0x02; // reset other bits
				}
				break;
			case 3:// addresses, nameservers, routes, dhcp4, dhcp6 at this level
			{
				if (state & 0x02){
					if (str.rfind("addresses:",0)==0){ // can be block or flow style
						net->DHCP = false;
						// check for 'flow' style
						boost::regex re("^addresses:\\s*\\[(\\d+\\.\\d+\\.\\d+\\.\\d+)/(\\d+)\\]$"); // single IPv4 address ONLY
						if (boost::regex_search(str,matches,re)){
							net->address=matches[1];
							net->netmask=prefix2netmask(matches[2]);
							// state doesn't change
						}
						else{// must be block, set state to parse address
							state = 0x01 | 0x02 | 0x04;
						}
						
					}
					else if (str.rfind("nameservers:",0)==0){
						state = 0x01 | 0x02 | 0x08;
					}
					else if (str=="routes:"){
						state =  0x01 | 0x02 | 0x10;
					}
					else if (str=="dhcp4:"){ // FIXME
						net->DHCP = true;
					}
				}
				break;
			}
			case 4:
			{
				if (state & 0x04){
					boost::regex re("^-\\s*(\\d+\\.\\d+\\.\\d+\\.\\d+)/(\\d+)$"); // single IPv4 address ONLY
					if (boost::regex_search(str,matches,re)){
						net->address=matches[1];
						net->netmask=prefix2netmask(matches[2]);
						state = 0x01 | 0x02 | 0x04;
					}
				}
				else if (state & 0x08){ // looking for nameserver addresses
					boost::regex re("^addresses:\\s*\\[(.+)\\]$"); 
					if (boost::regex_search(str,matches,re)){ // flow style
						std::string ips = matches[1];
						boost::regex ipre("\\d+\\.\\d+\\.\\d+\\.\\d+"); // note: no simple way to return multiple matches
						boost::sregex_iterator it{ips.begin(), ips.end(), ipre }, itEnd;
						std::for_each( it, itEnd, [net]( const boost::smatch& m ){
							//std::cout << m[0] << std::endl;
							net->nameservers.push_back(m[0]);
						});
					} // must be block style
					else{
						state = 0x01 | 0x02 | 0x08;
					}
				}
				else if (state & 0x10){ // looking for the default route
					boost::regex re("^-\\s+to:\\s*default$"); 
					if (boost::regex_search(str,matches,re)){
						state = 0x01 | 0x02 | 0x20; 
					}
				}
				break;
			}
			case 5:
			{
				if (state & 0x08){ // looking for nameserver addresses, block style
					boost::regex re("^-\\s*(\\d+\\.\\d+\\.\\d+\\.\\d+)$"); // single IPv4 address ONLY 
					if (boost::regex_search(str,matches,re)){
						net->nameservers.push_back(matches[1]);
						//cout << matches[1] << endl;
						// no state change reqd
					}
				}
				else if (state & 0x020){ // looking for the default route
					boost::regex re("^via:\\s*(\\d+\\.\\d+\\.\\d+\\.\\d+)$"); // single IPv4 address ONLY
					if (boost::regex_search(str,matches,re)){
						net->gateway = matches[1];
						//cout << net->gateway << endl;
						// no state change reqd
					}
				}
			}
			
			default:
				break;
		}
		l=l+1;
	}

	for (l=0;l<nets.size();l++){
		DBGMSG(debugStream,TRACE,"name " << nets.at(l)->name);
		DBGMSG(debugStream,TRACE,"addr " << nets.at(l)->address);
		DBGMSG(debugStream,TRACE,"mask " << nets.at(l)->netmask);
		for (unsigned int n=0;n<nets.at(l)->nameservers.size();n++){
			DBGMSG(debugStream,TRACE," ns  " << nets.at(l)->nameservers.at(n));
		}
		DBGMSG(debugStream,TRACE,"gw  " << nets.at(l)->gateway);
	}
	
	// Now we have to identify the interfaces according to the device names set in the configuration file
	for (l=0;l<nets.size();l++){
		if (nets.at(l)->name  == LANIFname[0]){
			idxLAN[0] = l;
		}
	}

	// If we didn't find 'ethernets' then we have an ultrabasic netplan which means that DHCP is the default
	if (nets.size() > 0){
		if (idxLAN[0] >= 0) // found it, so use what was specified
			addressAssignmentLAN[0]=nets.at(idxLAN[0])->DHCP?DHCP:Static;
		else // otherwise,DHCP
			addressAssignmentLAN[0]=DHCP;
	}
	else{ // no ethernets, all DHCP
		addressAssignmentLAN[0]=DHCP;
	}
	
	DBGMSG(debugStream,TRACE,"network protocol " << addressAssignmentLAN[0]);
	
}

bool LCDMonitor::writeNetPlanConfig(int ifID){
	std::string tmp;
	
	// Make temporary files and rename when done.
	// Note that temporary files are made in the same directory
	// as the target because rename() does not work across devices (partitions)
	// network
	
	std::string ftmp = netCfg + ".tmp"; 
	ofstream fout(ftmp.c_str());
	if (!fout.good()){
		log("Can't open " + ftmp);
		return false;
	}
	
	fout << "# Configuration created by lcdmonitor" << endl;
	fout << "# Hand edits may be overwritten" << endl;
	
	if (nets.at(ifID)->DHCP){
		fout << "network:" << endl;
		fout << "  ethernets:" << std::endl;
		fout << "    " << nets.at(ifID)->name << ":" << std::endl;
		fout << "      dhcp4: true" << std::endl;
	}
	else{
		fout << "network:" << std::endl;
		fout << "  ethernets:" << std::endl;
		fout << "    " << nets.at(ifID)->name << ":" << std::endl;
		fout << "      dhcp4: false" << std::endl;
		fout << "      addresses:" << std::endl;
		fout << "        - " << nets.at(ifID)->address << "/" << netmask2prefix(nets.at(ifID)->netmask) << std::endl;
		fout << "      routes:" << std::endl;
		fout << "        - to: default" << std::endl;
		fout << "          via: " << nets.at(ifID)->gateway << std::endl;     
		fout << "      nameservers:" << std::endl; 
		if (nets.at(ifID)->nameservers.at(1) == "0.0.0.0"){
			fout << "        addresses: [" << nets.at(ifID)->nameservers.at(0) << "]" << std::endl;
		}
		else{
			fout << "        addresses: [" << nets.at(ifID)->nameservers.at(0) << "," << nets.at(ifID)->nameservers.at(1) << "]" << std::endl;
		}
	//       search:
	//         - nmi.measurement.gov.au
		//version: 2
	}
	fout.close();
	
	int retval;
	if (0 != (retval =rename(ftmp.c_str(),netCfg.c_str()))){
		DBGMSG(debugStream,TRACE,"Rename of " << ftmp << " to " << netCfg << " failed err = " << errno);
		return false;
	}
	
	// netplan is fussy about permissions
	chmod(netCfg.c_str(),S_IRUSR | S_IWUSR);
	
	return true;
}

void LCDMonitor::networkConfigDHCP0(){
	networkConfigDHCP(0);
}

void LCDMonitor::networkConfigStaticIP40(){
	networkConfigStaticIP4(0);
}


// Disabled for OpenTTP
bool LCDMonitor::restartNetworking()
{
	bool ret= false;
	
	clearDisplay();

	updateLine(1,"Restarting network");


#ifdef NMCLI
	// note that CentOS7+ have /bin as a symlink to /usr/bin, so all good
	runSystemCommand("/bin/nmcli connection reload  && /bin/nmcli networking off && /bin/nmcli networking on","Restarted OK","Restart failed !");
	sleep(1);
#else
// runSystemCommand("/bin/systemctl restart systemd-networkd","Restarted OK","Restart failed !");
	runSystemCommand("/usr/sbin/netplan apply","Restarted OK","Restart failed !"); // Ubuntu only
	sleep(1);
#endif
	clearDisplay();
	updateLine(1,"Trying ssh restart");
	runSystemCommand("/bin/systemctl try-restart ssh","Restarted OK","Restart failed !");
	sleep(1);
	
	clearDisplay();
	updateLine(1,"Restarting NTP");
	runSystemCommand(ntpRestartCommand,"Restarted OK","Restart failed !");
	if (NTPDaemon == CHRONYD){
		sleep(1);
		clearDisplay();
		updateLine(1,"Restarting gpsd");
		runSystemCommand(gpsdRestartCommand,"gpsd restarted OK","gpsd restart failed");
	}
	sleep(1);

	return ret;
}

void LCDMonitor::LCDConfig()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IntensityWidget *iw = new IntensityWidget("Intensity",intensity,dlg);
	iw->setGeometry(0,0,20,1);
	iw->setFocusWidget(true);
	ContrastWidget *cw = new ContrastWidget("Contrast",contrast,dlg);
	cw->setRange(0,255);
	cw->setNumSteps(10);
	cw->setGeometry(0,1,20,1);
	std::string help = "  Adjust with ";
	help += char(225);
	help += " ";
	help += char(223);
	Label *l= new Label(help,dlg);
	l->setGeometry(0,2,20,1);

	WidgetCallback<Dialog> *cb = new WidgetCallback<Dialog>(dlg,&Dialog::ok);
	Button *b = new Button("OK",cb,dlg);
	b->setGeometry(5,3,2,1);

	cb = new WidgetCallback<Dialog>(dlg,&Dialog::cancel);
	b = new Button("Cancel",cb,dlg);
	b->setGeometry(10,3,6,1);

	bool ret = execDialog(dlg);
	if (ret)
	{
		if (intensity != iw->value())
		{
			intensity = iw->value();
			std::string sbuf;
			ostringstream ossbuf(sbuf);
			ossbuf << intensity;
			updateConfig("ui","lcd intensity",ossbuf.str());
		}
		if (contrast != cw->value())
		{
			contrast = cw->value();
			std::string sbuf;
			ostringstream ossbuf(sbuf);
			ossbuf << contrast;
			updateConfig("ui","lcd contrast",ossbuf.str());
		}
	}
	else // we cancelled
	{
		// bit ugly but ...
		COMMAND_PACKET cmd;
		cmd.command=13;
		cmd.data[0]=contrast;
		cmd.data_length=1;
		sendCommand(cmd);

		cmd.command=14;
		cmd.data[0]=intensity;
		cmd.data_length=1;
		sendCommand(cmd);
	}
	delete dlg;
}

void LCDMonitor::LCDBacklightTimeout()
{
	// Set up display
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	
	std::string messg = "Backlight timeout:";
	Label *m = new Label(messg,dlg);
	m->setGeometry(0,0,18,1);
	
	NumberWidget *nw = new NumberWidget(displaytimeout,dlg);
	nw->setGeometry(8,1,4,1);
	nw->setFocusWidget(true);
	
	std::string minutes = "seconds";
	Label *n = new Label(minutes,dlg);
	n->setGeometry(13,1,7,1);
	
	std::string help = "  Adjust with ";
	help += char(222); // Up arrow
	help += " ";
	help += char(224); // Down arrow
	Label *l= new Label(help,dlg);
	l->setGeometry(0,2,20,1);

	WidgetCallback<Dialog> *cb = new WidgetCallback<Dialog>(dlg,&Dialog::ok);
	Button *b = new Button("OK",cb,dlg);
	b->setGeometry(5,3,2,1);

	cb = new WidgetCallback<Dialog>(dlg,&Dialog::cancel);
	b = new Button("Cancel",cb,dlg);
	b->setGeometry(10,3,6,1);
	
	bool ret = execDialog(dlg);
	
	if (ret){
		// store new value for timeout in settings file and assign new value to global variable
		if (nw->value() != displaytimeout){
			displaytimeout = nw->value();
			std::string sbuf;
			ostringstream ossbuf(sbuf);
			ossbuf << displaytimeout;
			updateConfig("ui","lcd timeout",ossbuf.str());
		}
	}
	else{ // we cancelled
		// do nothing...
	}
	delete dlg;
}

void LCDMonitor::setGPSDisplayMode()
{
	if (displayMode==GPS) return;

	displayMode=GPS;
	MenuItem *mi = displayModeM->itemAt(midGPSDisplayMode);
	mi->setChecked(true);
	mi = displayModeM->itemAt(midNTPDisplayMode);
	mi->setChecked(false);
	mi = displayModeM->itemAt(midGPSDODisplayMode);
	mi->setChecked(false);
	#ifdef MULTIRX
	mi = displayModeM->itemAt(midGLOBDDisplayMode);
	mi->setChecked(false);
	#endif

	updateConfig("ui","display mode","GPS");

	clearDisplay();
}

void LCDMonitor::setNTPDisplayMode()
{
	if (displayMode==NTP) return;

	displayMode=NTP;
	MenuItem *mi = displayModeM->itemAt(midNTPDisplayMode);
	mi->setChecked(true);
	mi = displayModeM->itemAt(midGPSDisplayMode);
	mi->setChecked(false);
	mi = displayModeM->itemAt(midGPSDODisplayMode);
	mi->setChecked(false);
	#ifdef MULTIRX
	mi = displayModeM->itemAt(midGLOBDDisplayMode);
	mi->setChecked(false);
	#endif
	lastNTPtrafficPoll.tv_sec=0;

	updateConfig("ui","display mode","NTP");

	clearDisplay();
}

void LCDMonitor::setRefDisplayMode()
{
	if (displayMode==REF) return;

	displayMode=REF;
	MenuItem *mi = displayModeM->itemAt(midGPSDODisplayMode);
	mi->setChecked(true);
	mi = displayModeM->itemAt(midGPSDisplayMode);
	mi->setChecked(false);
	mi = displayModeM->itemAt(midNTPDisplayMode);
	mi->setChecked(false);
	#ifdef MULTIRX
	mi = displayModeM->itemAt(midGLOBDDisplayMode);
	mi->setChecked(false);
	#endif

	updateConfig("ui","display mode","GPSDO");

	clearDisplay();
}

#ifdef MULTIRX
void LCDMonitor::setGLOBDDisplayMode()
{
	if (displayMode==GLOBD) return;
	//
	displayMode=GLOBD;
	MenuItem *mi = displayModeM->itemAt(midGPSDisplayMode);
	mi->setChecked(false);
	mi = displayModeM->itemAt(midNTPDisplayMode);
	mi->setChecked(false);
	mi = displayModeM->itemAt(midGPSDODisplayMode);
	mi->setChecked(false);
	mi = displayModeM->itemAt(midGLOBDDisplayMode);
	mi->setChecked(true);
	//
	updateConfig("ui","display mode","GLOBD");
	//
	clearDisplay();
}
#endif

void LCDMonitor::restartRx()
{
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm Rx restart");
	bool ret = execDialog(dlg);
	if (ret)
	{
		clearDisplay();

		updateLine(1,"  Restarting Rx");
		// first kill the logging process if it is running
		struct stat statbuf;
		if ((0 == stat(gpsLoggerLockFile.c_str(),&statbuf)))
		{
			std::ifstream fin(gpsLoggerLockFile.c_str());
			if (!fin.good())
			{
				log("Couldn't open " + gpsLoggerLockFile);
				goto fail;
			}
			pid_t pid = 0;
			// ELM: lock file now contains app name and PID - this is consequence
			// of making all the lock files consistent with the TFLibrary.
			std::string pids;
			fin >> pids;
			fin >> pid;
			fin.close();
			
			if (pid > 0) // be careful here, since we are superdude
				kill(pid,SIGTERM);
			else
				goto fail;
			
			sleep(2); // wait a bit for OS to do its thing
		}
		
		DBGMSG(debugStream,TRACE,  gpsRxRestartCommand );
		int sysret = system(gpsRxRestartCommand.c_str());
		DBGMSG(debugStream,TRACE, "system() returns " << sysret);
		if (sysret == -1)
			goto fail;
		else
		{
			updateLine(2,"  Done");
			log("Rx restarted");
		}
		sleep(2);

		delete dlg;
		return;
	} 
	else //cancelled the dialog
	{
		delete dlg;
		return;
	}

	fail:
		updateLine(2,"  Restart failed");
		log("Rx restart failed");
		sleep(2);
		delete dlg;
}

void LCDMonitor::restartNTP()
{
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm NTP restart");
	bool ret = execDialog(dlg);
	if (ret){
		clearDisplay();
		updateLine(1,"Restarting NTP");
		runSystemCommand(ntpRestartCommand,"NTP restarted","NTP restart failed");
		if (NTPDaemon == CHRONYD){
			sleep(1);
			clearDisplay();
			updateLine(1,"Restarting gpsd");
			runSystemCommand(gpsdRestartCommand,"gpsd restarted","gpsd restart failed");
		}
	}
	delete dlg;
}

void LCDMonitor::reboot()
{

	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm reboot");
	bool ret = execDialog(dlg);
	if (ret)
	{
		clearDisplay();
		runSystemCommand(rebootCommand,"Rebooting ...","Reboot failed !");
	}
	delete dlg;
}

void LCDMonitor::poweroff()
{
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm poweroff");
	bool ret = execDialog(dlg);
	if (ret)
	{
		clearDisplay();
		runSystemCommand(poweroffCommand,"Powering off ...","Poweroff failed !");
	}
	delete dlg;
}

void LCDMonitor::clearDisplay()
{
	for (int i=0;i<4;i++)
		status[i]="";
	outgoing_response.command = 6;
	outgoing_response.data_length = 0;
	send_packet();
	getResponse();
}

void LCDMonitor::run()
{
	clearDisplay();
	std::time_t ts = std::time(NULL);
	displaybacklightoff = false;
	COMMAND_PACKET cmd;
	while (1)
	{
		// use usleep timing otherwise the displayed time jumps 1s occasionally because of rounding
		struct timeval tv;
		gettimeofday(&tv,0);
		usleep(1000000-tv.tv_usec+10000); // wait until a bit after the second rollover so we're not fighting ntpd 
		if(packetReceived())
		{
			// check the packet type - timeouts can generate unexpected packets
			if (incoming_command.command==0x80) // key events only
			{
				// Turn backlight on if it was off
				if(displaybacklightoff)
				{
					cmd.command=14;
					cmd.data[0]=intensity;
					cmd.data_length=1;
					sendCommand(cmd);
					displaybacklightoff=false;
					ts = std::time(NULL);
				}
				ShowReceivedPacket();
				clearDisplay();
				execMenu();
				lastLazyCheck=0; // trigger immediate update
				clearDisplay();
			}
		}
		else
			showStatus();
		// if LCD backlight is of, turn it off if timeout is reached
		if(displaytimeout != 0)
			if(std::time(NULL) > (ts+displaytimeout) && !displaybacklightoff)
			{
				cmd.command=14;
				cmd.data[0]=0;
				cmd.data_length=1;
				sendCommand(cmd);
				displaybacklightoff=true;
			}
	}
}

void LCDMonitor::showStatus()
{
	char buf[21];/* maximum of 20 characters per line */

	// Construct a message for display 
	time_t tnow = time(NULL);
	struct tm *tmnow = gmtime(&tnow);
	sprintf(buf,"%d-%02d-%02d %02d:%02d:%02d",
		tmnow->tm_year+1900,tmnow->tm_mon+1,tmnow->tm_mday,
		tmnow->tm_hour,tmnow->tm_min,tmnow->tm_sec);

	updateLine(0,buf);

	// only run these checks every 10s or so
	if (tnow - lastLazyCheck > 10)
	{

		touchLock();

		switch (displayMode)
		{
			case NTP:
			{
				//printf("Getting time of day\n");
				gettimeofday(&currNTPtrafficPoll,NULL);
				if (currNTPtrafficPoll.tv_sec - lastNTPtrafficPoll.tv_sec < 60) break;

				int oldpkts=-1,newpkts=-1,badpkts=-1;
				//printf("Get NTP stats\n");
				// OK getNTPstats fails...
				getNTPstats(&oldpkts,&newpkts,&badpkts);
				if (oldpkts >=0 && newpkts >=0 && badpkts >=0)
				{

					currNTPPacketCount=oldpkts+newpkts+badpkts;
					if (currNTPPacketCount < lastNTPPacketCount) // counter rollover
					{
						lastNTPtrafficPoll = currNTPtrafficPoll;
						lastNTPPacketCount = currNTPPacketCount; 
						break; // no update
					}
					if (lastNTPtrafficPoll.tv_sec>0) // got an interval
					{
						double pktrate = 60.0 * (currNTPPacketCount -  lastNTPPacketCount)/
							(currNTPtrafficPoll.tv_sec - lastNTPtrafficPoll.tv_sec +
							 1.0E-6*(currNTPtrafficPoll.tv_usec - lastNTPtrafficPoll.tv_usec));
						std::string sbuf;
						ostringstream ossbuf(sbuf);
						ossbuf.precision(1); // just one after the decimal point
						ossbuf << std::fixed << pktrate << " pkts/min";
						updateLine(1,ossbuf.str().c_str());
						lastNTPtrafficPoll = currNTPtrafficPoll;
						lastNTPPacketCount = currNTPPacketCount; 
					}
					else // first time thru
					{
						updateLine(1,"???? pkts/min");
						lastNTPtrafficPoll = currNTPtrafficPoll;
						lastNTPPacketCount = currNTPPacketCount; 
					}
					//printf("Should have NTP stats now\n");

				}
				break;
			}
			case GPS:
			{
				std::string prn="";
				int nsats=0;
				bool unexpectedEOF;
				checkRx(&nsats,prn,&unexpectedEOF); 
				// Note: NV08C returns GPS and GLONASS sats in prn string, so we must only
				//       parse the GPS sats - nsats contain the number of GPS sats
				if (unexpectedEOF)
					DBGMSG(debugStream,TRACE,"Unexpected EOF from checkGPS");

				if (showPRNs && !unexpectedEOF){
					// split this over two lines
					// Have 15 characters per line == 6 per line
					std::vector<std::string> sprns;
					boost::split(sprns,prn,is_any_of(",")); // note that if no split, then input is returned
					// Reduce the length of the array (if necessary) so that we retain only the GPS PRN numbers
					sprns.resize(nsats);
					std::vector<int> prns;
					for (unsigned int i=0;i<sprns.size();i++){
						prns.push_back(atoi(sprns.at(i).c_str()));
					}
					sort(prns.begin(),prns.end());
					int nprns = prns.size();
					if (nprns > 6) nprns=6;
					std::string sbuf;
					ostringstream ossbuf(sbuf);
					ossbuf << "SV";

					for (int s=0;s<nprns;s++){
						ossbuf.width(3);
						ossbuf << prns[s];
					}
					updateLine(1,ossbuf.str().c_str());

					nprns = prns.size();
					if (nprns > 12) nprns=12; // can only do 12
					nprns -=6; // number left to show
					ossbuf.clear(); // clear any errors 
					ossbuf.str("");
					ossbuf << "  ";
					for (int s=0;s<nprns;s++){
						ossbuf.width(3);
						ossbuf << prns[s+6];
					}
					updateLine(2,ossbuf.str().c_str());
				}
				else if (!unexpectedEOF)
				{
					sprintf(buf,"GPS sats=%d",nsats);
					updateLine(1,buf);
				}
				break;
			}
			case REF:
			{
				std::string status,ffe,EFC,health;
				bool unexpectedEOF;
				/*
				if (!checkGPSDO(status,ffe,EFC,health,&unexpectedEOF))
					cout << "checkGPSDO() returned false\n";
				else
					cout << "checkGPSDO() returned true\n";
				*/
				if(checkRef(status,ffe,EFC,health,&unexpectedEOF))
				{
					if (unexpectedEOF){
						DBGMSG(debugStream,TRACE, "Unexpected EOF from checkGPSDO");
					}
					else{
						if (reference== ULN1100){
							status = "GPSDO: " + status;
							if (status.length() > 20) status.resize(20);
							updateLine(1,status);
							std::string buf;
							if (ffe.length() > 7) ffe.resize(7);
							if (EFC.length() > 4) EFC.resize(4);
							buf = "ffe:" + ffe + " EFC:" + EFC;
							size_t pos = health.find("-");
							health.resize(pos);
							health = "Health: " + health;
							if (health.length() > 20) health.resize(20);
							if(showHealth)
								updateLine(2,health);
							else
								updateLine(2,buf);
							showHealth = !showHealth;
						}
						else if (reference== LCXO){
							updateLine(1,"GPSDO:");
							// The status file structure is very different. A different approach
							// is used to display GPSDO parameters
							switch(statusline)
							{
								case(0):
									if(status.length() > 20) status.resize(20);
									updateLine(2,status);
									break;
								case(1):
									ffe = "FFE: "+ffe;
									if(ffe.length() > 20) ffe.resize(20);
									updateLine(2,ffe);
									break;
								case(2):
									EFC = "EFC: "+EFC;
									if(EFC.length() > 20) EFC.resize(20);
									updateLine(2,EFC);
									break;
								case(3):
									if(health.length() > 20) health.resize(20);
									updateLine(2,health);
									break;
							}
							statusline++;
							if(statusline >= 4) statusline = 0;
						}
						else if (reference== Furuno){
							status = "DOP: " + status;
							if (status.length() > 20) status.resize(20);
							updateLine(1,status);
							std::string buf;
							if (ffe.length() > 10) ffe.resize(10);
							buf = "ffe:" + ffe;
							//if(health.length() > 20) health.resize(20);
							//health = "Health: " + health;
							if (health.length() > 20) health.resize(20);
							if(showHealth)
								updateLine(2,health);
							else
								updateLine(2,buf);
							showHealth = !showHealth;
						}
					}
				}
				else // most likely stale file...
				{
					updateLine(1,"GPSDO:");
					updateLine(2,"");
				}
				break;
			} //
#ifdef MULTIRX
			case GLOBD:
			{
				std::string GLOsats = "", BDsats = "";
				bool unexpectedEOF;
				if(checkGLOBD(GLOsats,BDsats,&unexpectedEOF))
				{
					if (unexpectedEOF){
						DBGMSG(debugStream,TRACE,"unexpected EOF from checkGLOBD");
					}
					else
					{
						std::vector<std::string> sprns;
						std::vector<int> prns;
						std::string sbuf;
						ostringstream ossbuf(sbuf);
						int n1 = 4,bufwd,maxn;
						if(showGLOBD){ // Show GLONASS sats
							boost::split(sprns,GLOsats,is_any_of(","));
							ossbuf << "GLONASS";
							bufwd = 3;
							maxn = 11;
						}
						else{ // Show Beidou sats
							boost::split(sprns,BDsats,is_any_of(","));
							ossbuf << "Beidou ";
							bufwd = 3; // was 4 when 2xx was used as prn no
							maxn = 11; // was 9 when 2xx was used as prn no
						}
						for (unsigned int i=0;i<sprns.size();i++){
							
							int prn = atoi(sprns.at(i).c_str());
							if (showGLOBD){
								// Due to problems with GLONASS visibility, the SMT360 receiver is now configured to also
								// track GPS (but output a GLONASS aligned PPS). This means that the PRN list needs to be filtered to remove non-GLONASS SV
								// For the SMT360 (and ublox) the SV IDs are 65-96 (these are NMEA standard)
								if (prn > 65 && prn <= 96){
									prns.push_back(prn);
								}
							}
							else{ // BDS
								prns.push_back(prn - 200); // NavSpark: subtract 200 from prn number to get sv number for Beidou satellites
							}
						}	
						sort(prns.begin(),prns.end());
						int nprns = prns.size();
						if (nprns > n1) nprns = n1;
						for (int s=0;s<nprns;s++){
							ossbuf.width(bufwd);
							ossbuf << prns[s];
						}
						updateLine(1,ossbuf.str().c_str());
						nprns = prns.size();
						if (nprns > maxn) nprns = maxn;
						nprns -= n1; // number left to show
						ossbuf.clear(); // clear any errors 
						ossbuf.str("");
						for (int s=0;s<nprns;s++){
							ossbuf.width(bufwd);
							ossbuf << prns[s+n1];
						}
						updateLine(2,ossbuf.str().c_str());
						showGLOBD = !showGLOBD;
					}
				}
				else // most likely stale file
				{
					updateLine(1,"GLONASS - nothing");
					updateLine(2,"Beidou - nothing");
				}
				break;
			}
#endif
		}
		
		//if (GPSOK)
		//	updateStatusLED(1,GreenOn);
		//else
		//	updateStatusLED(1,RedOn);

		if (checkAlarms())
		{
			updateLine(3,"SYSTEM OK");
			updateStatusLED(3,GreenOn);
		}
		else
		{
			updateLine(3,"SYSTEM ALARM");
			updateStatusLED(3,RedOn);
		}
		lastLazyCheck=tnow;
	}

	// leap second status
	if (displayMode == NTP)
	{
		struct timex tx;
		tx.modes=0;
		int clkstate=adjtimex(&tx);
		switch (clkstate)
		{
			case TIME_OK:
				snprintf(buf,20,"Time OK");
				break;
			case TIME_INS:
				snprintf(buf,20,"Leap second today");
				break;
			case TIME_DEL:
				snprintf(buf,20,"Leap second today");
				break;
			case TIME_OOP:
				snprintf(buf,20,"Leap second now");
				break;
			case TIME_WAIT:
				snprintf(buf,20,"Leap sec occurred");
				break;
			case TIME_BAD:
				snprintf(buf,20,"Unsynchronized");
				break;	
		}
		updateLine(2,buf);
	}

}

void LCDMonitor::execMenu()
{
	bool showMenu=true;
	std::stack<Menu *> menus;
	
	readNetPlanConfig();
	
	int currRow=0;
	statusLEDsOff();
	showCursor(true);

	menus.push(menu);
	Menu *currMenu=menus.top();

	startTimer(300);
	while (showMenu && !LCDMonitor::timeout)
	{
		// show the menu
		clearDisplay();
		int numItems=currMenu->numItems();
		for (int i=0;i<numItems;i++)
		{
			std::string buf = ">" + currMenu->itemAt(i)->text();
			if (buf.length() < 20) // pad the string
				buf+=std::string(20-buf.length(),' ');
			if (currMenu->itemAt(i)->checked())
				buf.at(19)='*';
			updateLine(i,buf);
		}
		updateCursor(currRow,0);
		while (!LCDMonitor::timeout)
		{
			usleep(10000);
			if(packetReceived())
			{
				stopTimer();
				startTimer(300);
				if(incoming_command.command==0x80)
				{
					if (incoming_command.data[0]==10 || incoming_command.data[0]==11)// RIGHT RELEASE/ENTER - next menu/exec item
					{
						DBGMSG(debugStream,TRACE, "selected " << currRow);
						MenuItem *currItem = currMenu->itemAt(currRow);
						if (currItem)
						{
							if (currItem->isMenu())
							{
								DBGMSG(debugStream,TRACE, "selected menu " << currRow << "(menu)");
								currMenu = (Menu *) currItem;
								menus.push(currMenu);
								currRow=0;
							}
							else
							{
								stopTimer();
								currItem->exec();
								startTimer(300);
							}
						}

						break;
					}
					else if (incoming_command.data[0]==7) // UP RELEASE - prev item
					{
						if (currRow>0) currRow--;
						updateCursor(currRow,0);
					}
					else if (incoming_command.data[0]==8) // DOWN RELEASE  - next itme
					{
						if (currRow<numItems-1) currRow++;
						updateCursor(currRow,0);
					}
					else if (incoming_command.data[0]==9) // LEFT_RELEASE - prev menu
					{
						// What's on the stack
						if (menus.size() == 1)
      				showMenu=false; // bybye
						else
						{
							menus.pop();
							currMenu=menus.top();
							currRow=0;
						}
						break;
					}
					else if (incoming_command.data[0]==12) // EXIT_RELEASE - back to main screen
					{
						showMenu=false;
						break;
					}
				}
			}
		}
	}
	showCursor(false);
	stopTimer();
}

bool LCDMonitor::execDialog(Dialog *dlg)
{
	// dispatch events to the dialog
	// until the widget returns OK or ESC/CANCEL

	int currRow=0;
	int currCol=0;
	dlg->focus(&currCol,&currRow);

	bool exec=true;
	std::vector<std::string> display;
	std::string blank;
	blank.append(20,' ');

	display.push_back(blank);display.push_back(blank);display.push_back(blank);display.push_back(blank);

	bool retval = false;

	DBGMSG(debugStream,TRACE,"");
	startTimer(300);
	while (exec && !LCDMonitor::timeout)
	{

		repaintWidget(dlg,display,true);

		updateCursor(currRow,currCol);
		while (!LCDMonitor::timeout)
		{
			usleep(10000);

			if(packetReceived())
			{
				stopTimer();
				startTimer(300);

				if(incoming_command.command==0x80)
				{
					int event=0;

					if (incoming_command.data[0]==7) // UP RELEASE
						event = KeyEvent::KeyRelease|KeyEvent::KeyUp;
					else if (incoming_command.data[0]==8) // DOWN RELEASE
						event=KeyEvent::KeyRelease|KeyEvent::KeyDown;
					else if (incoming_command.data[0]==9) // LEFT RELEASE
						event=KeyEvent::KeyRelease|KeyEvent::KeyLeft;
					else if (incoming_command.data[0]==10) // RIGHT RELEASE
						event=KeyEvent::KeyRelease|KeyEvent::KeyRight;
					else if (incoming_command.data[0]==11 )// ENTER
						event = KeyEvent::KeyEnter;
					else if (incoming_command.data[0]==12) // EXIT_RELEASE
					{
						exec=false;
						retval=false;
						DBGMSG(debugStream,TRACE,"Dialog cancelled");
						break;
					}

					if (event != 0)
					{
						KeyEvent ke(event,currCol,currRow);
						dlg->keyEvent(ke);
						if (dlg->finished())
						{
							retval = dlg->exitCode()==Dialog::OK;
							exec=false;
							break;
						}
						else
						{
							DBGMSG(debugStream,TRACE, "!!!!repainting");
							repaintWidget(dlg,display);
							dlg->focus(&currCol,&currRow);
							updateCursor(currRow,currCol);
						}
					}

				}
			}
		}
	}
	DBGMSG(debugStream,TRACE, "dialog finished");
	stopTimer();
	return retval;
}

void LCDMonitor::sendCommand(COMMAND_PACKET &cmd)
{
	DBGMSG(debugStream,TRACE,"");
	outgoing_response.command =cmd.command;
	outgoing_response.data_length=cmd.data_length;
	for (unsigned int i=0;i<cmd.data_length;i++)
		outgoing_response.data[i]=cmd.data[i];
	send_packet();
	getResponse();
}

//
// private
//

void LCDMonitor::signalHandler(int sig)
{
	switch (sig)
	{
		case SIGALRM: 
			DBGMSG(debugStream,TRACE, "SIGALRM");
			LCDMonitor::timeout=true;
			break;// can't call Debug()
		case SIGTERM: case SIGQUIT: case SIGINT: case SIGHUP:
			DBGMSG(debugStream,TRACE, "SIGxxx");
			delete app;
			exit(EXIT_SUCCESS);
			break;
	}

}

void LCDMonitor::startTimer(long secs)
{

	struct itimerval itv; 

	LCDMonitor::timeout=false;

	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0; // this stops the timer on timeout
	itv.it_value.tv_sec=secs;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Start timer failed"); 

}

void LCDMonitor::stopTimer()
{
	struct itimerval itv;
	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0;
	itv.it_value.tv_sec=0;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Stop timer failed");  
}

void LCDMonitor::init()
{

	FILE *fd;
	pid_t oldpid;

	DBGMSG(debugStream,TRACE,"");
	
	logFile = DEFAULT_LOG_FILE;

	lockFile = DEFAULT_LOCK_FILE;

	// Check that there isn't already a process running 
	if ((fd=fopen(lockFile.c_str(),"r"))){
		fscanf(fd,"%i",&oldpid);
		fclose(fd);
		// If the process has not exited properly, there will be a stale lock file
		// If the system has rebooted there may a new process with the PID in the lock file
		// so we check /proc
		ostringstream sbuf;
		sbuf << "/proc/" << oldpid;
		DIR* dir  = opendir(sbuf.str().c_str());
		if (dir){ // it's in /proc
			// check cmdline
			sbuf << "/cmdline";
			std::ifstream fin(sbuf.str().c_str());
			if (fin.good()){
				std::string cmdline;
				fin >> cmdline;
				fin.close();
				std::size_t found = cmdline.find("lcdmonitor");
				if (found!=std::string::npos){
					std::cerr << "lcdmonitor with pid " << oldpid << " is still running" << endl;
					exit(EXIT_FAILURE);
				}
			}
		}
		// otherwise, good to go.
	}

	log(PRETTIFIER);
	ostringstream sbuf;
	sbuf << "lcdmonitor v" << LCDMONITOR_VERSION << ", last modified " << LAST_MODIFIED; 
	log(sbuf.str());
	log(PRETTIFIER);

	/* make a new lock */
	if ((fd = fopen(lockFile.c_str(),"w"))){
		fprintf(fd,"%i\n",(int) getpid());
		fclose(fd);
	}
	else{
		log("failed to make a lock");
		exit(EXIT_FAILURE);
	}

	lastNTPtrafficPoll.tv_sec=0;
	lastNTPPacketCount=0;

	configure();
	
	readNetPlanConfig();
	
	// This will be true for compact systems
	NTPProtocolVersion=4;
	if (NTPDaemon == CHRONYD){
		NTPCLIMajorVersion = 4; // for chronyc
		NTPCLIMinorVersion = 2;
	}
	else if (NTPDaemon == NTPD){
		NTPCLIMajorVersion=2; // for ntpq
		NTPCLIMinorVersion=2; 
	}
	
	// Note: (Louis, 2016-10-25)
	//  This will not work with ntpq (ntpdc is now deprecated!)
	//  The mods below will only work with version 4 and newer!
	detectNTPVersion();
	
	if (NTPDaemon == CHRONYD){
		currPacketsTag="NTP packets received";
		oldPacketsTag=""; // not reported
		badPacketsTag=""; // not reported
	}
	else if (NTPDaemon == NTPD){
	
		if (4==NTPProtocolVersion){
			if (NTPCLIMajorVersion < 2){
				currPacketsTag="new version packets";
				oldPacketsTag="old version packets";
				badPacketsTag="unknown version number";
			}
			else{
				currPacketsTag="current version";
				oldPacketsTag="older version";
				badPacketsTag="bad length or format";
			}
		}
	}
	
	if(Serial_Init(PORT,BAUD)){
		DBGMSG(debugStream,TRACE, "Could not open port " << PORT << " at " << BAUD << " baud.");
		exit(EXIT_FAILURE);
	}
	else{
		DBGMSG(debugStream,TRACE,PORT << " opened at "<< BAUD <<" baud");
	}

	/* Clear the buffer */
	while(BytesAvail())
    GetByte();

	for (int i=0;i<4;i++){
		status[i]="";
		statusLED[i]=Unknown;
	}

	// use SIGALRM to detect lack of UI activity and return to status display

	sa.sa_handler = signalHandler;
	sigemptyset(&(sa.sa_mask)); // we block nothing 
	sa.sa_flags=0;

	sigaction(SIGALRM,&sa,NULL);

	sigaction(SIGTERM,&sa,NULL);

	sigaction(SIGQUIT,&sa,NULL);

	sigaction(SIGINT,&sa,NULL);

	sigaction(SIGHUP,&sa,NULL);

	lastLazyCheck=0; // trigger an immediate check

	statusLEDsOff();

	/* set up intensity and contrast */
	COMMAND_PACKET cmd;
	cmd.command=13;
	cmd.data[0]=contrast;
	cmd.data_length=1;
	sendCommand(cmd);

	cmd.command=14;
	cmd.data[0]=intensity;
	cmd.data_length=1;
	sendCommand(cmd);

}

void LCDMonitor::configure()
{

	char *stmp;
	std::string strtmp;
	int itmp;

	// set some sensible defaults
	displayMode = GPS;

	poweroffCommand="/usr/sbin/poweroff";
	rebootCommand="/usr/sbin/shutdown -r now";
	ntpRestartCommand="/usr/bin/systemctl restart chrony";
	gpsdRestartCommand="/usr/bin/systemctl restart gpsd";
	gpsRxRestartCommand="su - cvgps -c 'kickstart.py'";
	gpsLoggerLockFile="/home/cvgps/logs/rest.lock";

	NTPuser="ntp-admin";
	GPSCVuser="cvgps";
	cvgpsHome="/home/cvgps/";
	ntpadminHome="/home/ntp-admin/";

	NTPDaemon = CHRONYD;
	
// the default setup is for Pi5 + Ubuntu
	idxLAN[0] = -1;
	idxLAN[1] = -1;
	LANIFname[0] =  "";
	LANIFname[1]  = ""; // unavailable
	NPrenderer = "NetworkManager";
	NPversion = "2";
	customNetCfg = false;
	netCfg = "/etc/netplan/00-network.yaml";
	
#ifndef NETPLAN 
	DNSconf="/etc/resolv.conf";
	networkConf="/etc/sysconfig/network";// This is empty in CentOS7+
	netCfg="/etc/sysconfig/network-scripts/ifcfg-";// device name is appended
#endif
	
	
	sysInfoConf="/usr/local/etc/sysinfo.conf";
	receiverName="nv08";
	alarmPath="/home/cvgps/logs/alarms";
	refStatusFile="/home/cvgps/var/gpsdo.status";
	rxStatusFile="/home/cvgps/var/rx.status";
#ifdef MULTIRX
	GLONASSStatusFile="/home/cvgps/logs/rest.status";
	BeidouStatusFile="/home/cvgps/logs/navspark.status";
	showGLOBD = true;
#endif
	reference= ULN1100;
	
	string sysmonConfig("/home/cvgps/etc/sysmonitor.conf");
	string gpscvConfig("/home/cvgps/etc/gpscv.conf");

	showPRNs=false;

	intensity=80;
	contrast=95;
	displaytimeout=0; // Louis 2017-07-17, timeout for LCD backlight
	ListEntry *last;
	if (!configfile_parse_as_list(&last,configFile.c_str())){
		ostringstream msg;
		msg << "failed to read " << configFile;
		log(msg.str());
		exit(EXIT_FAILURE);
	}

	// General
	if (list_get_string_value(last,"General","Ntp user",&stmp)){
		NTPuser=stmp;
		ntpadminHome = "/home/"+NTPuser+"/";
	}
	else
		log("NTP user not found in config file");

	if (list_get_string_value(last,"General","ntp daemon",&stmp)){
		if (NULL !=  strstr(stmp,"ntpd-nmi")){ // note! our custom version of ntpd
			NTPDaemon = NTPD;
		}
		else if (NULL != strstr(stmp,"chronyd")){
			NTPDaemon = CHRONYD;
		}
	}
	
	//  now we can construct the command to restart NTP daemon
	ntpRestartCommand = "/usr/bin/systemctl restart ";
	if (NTPDaemon == NTPD){
		ntpRestartCommand += "ntpd";
	}
	else if (NTPDaemon == CHRONYD){
		ntpRestartCommand += "chrony";
	}
	
	if (list_get_string_value(last,"General","sysmonitor config",&stmp))
		sysmonConfig=stmp;
	else
		log("sysmon config not found in config file");

	// Network
	if (list_get_string_value(last,"Network","LAN1 interface",&stmp))
		LANIFname[0] = stmp;
	else
		log("LAN1 not found in config file");
	
	
	if (list_get_string_value(last,"Network","LAN2 interface",&stmp))
		LANIFname[1] = stmp;
	
	if (list_get_string_value(last,"Network","cfg",&stmp))
		netCfg = stmp;
	else
		log("cfg not found in config file");
	
#ifndef NETPLAN
	if (list_get_string_value(last,"Network","DNS",&stmp))
		DNSconf=stmp;
	else
		log("DNS not found in config file");

	if (list_get_string_value(last,"Network","Network",&stmp))
		networkConf=stmp;
	else
		log("Network not found in config file");

#endif
	
	// GPSCV
	if (list_get_string_value(last,"GPSCV","GPSCV user",&stmp)){
		GPSCVuser=stmp;
		ntpadminHome = "/home/"+GPSCVuser+"/";
	}
	else
		log("GPSCV user not found in config file");

	if (list_get_string_value(last,"GPSCV","gpscv config",&stmp))
		gpscvConfig=stmp;
	else
		log("GPSCV config not found in config file");

	if (list_get_string_value(last,"GPSCV","GPS restart command",&stmp))
		gpsRxRestartCommand=stmp;
	else
		log("GPS restart command not found in config file");

	if (list_get_string_value(last,"GPSCV","oscillator",&stmp)){
		strtmp = stmp;
		boost::to_upper(strtmp);
		if (strtmp=="FURUNO"){
			reference= Furuno;
		}
		else if(strtmp == "LCXO"){
			reference= LCXO;
		}
		else if(strtmp == "ULN1100"){
			reference= ULN1100;
		}
	}
	else{
		log("GPSCV reference not found in config file");
	}
	// OS
	if (list_get_string_value(last,"OS","reboot command",&stmp))
		rebootCommand= stmp;
	else
		log("Reboot command not found in config file");

	if (list_get_string_value(last,"OS","poweroff command",&stmp))
		poweroffCommand= stmp;
	else
		log("Poweroff command not found in config file");
	
	

	// UI
	if (list_get_int_value(last,"UI","show PRNs",&itmp))
		showPRNs = (itmp==1);
	else
		log("Show PRNs not found in config file");

	if (list_get_int_value(last,"UI","LCD intensity",&itmp)){
		intensity = itmp;
		if (intensity <0) intensity=0;
		if (intensity >100) intensity=100;
	}
	else
		log("LCD intensity not found in config file");

	if (list_get_int_value(last,"UI","LCD contrast",&itmp)){
		contrast= itmp;
		if (contrast <0) contrast=0;
		if (contrast >255) contrast=255;
	}
	else
		log("LCD contrast not found in config file");

	if (list_get_int_value(last,"UI","LCD timeout",&itmp)){
		displaytimeout = itmp;
		if (displaytimeout < 0) displaytimeout = 0;
		if (displaytimeout > 3600) displaytimeout = 3600;
	}
	else
		log("LCD backlight timeout not found in config file");

	if (list_get_string_value(last,"UI","display mode",&stmp)){
		boost::to_upper(stmp);
		if (0==strcmp(stmp,"GPS"))
			displayMode = GPS;
		else if (0==strcmp(stmp,"NTP"))
			displayMode = NTP;
		else if (0==strcmp(stmp,"GPSDO"))
			displayMode = REF;
#ifdef MULTIRX
		else if (0==strcmp(stmp,"GLOBD"))
			displayMode = GLOBD;
#endif
		else{
			ostringstream msg;
			msg << "Unknown display mode " << stmp;
			log(msg.str());
		}
	}

	list_clear(last);

	//
	// Parse gpscv.conf
	//

	if (!configfile_parse_as_list(&last,gpscvConfig.c_str())){
		ostringstream msg;
		msg << "failed to read " << gpscvConfig;
		log(msg.str());
		exit(EXIT_FAILURE);
	}

	if (list_get_string_value(last,"receiver","model",&stmp))
		receiverName=stmp;
	else
		log("receiver type not found in gpscv.conf");

	if (list_get_string_value(last,"receiver","lock file",&stmp))
		gpsLoggerLockFile=relativeToAbsolutePath(stmp,cvgpsHome);
	else
		log("receiver:lock file not found in gpscv.conf");

	if (list_get_string_value(last,"receiver","status file",&stmp))
		rxStatusFile=relativeToAbsolutePath(stmp,cvgpsHome);
	else
		log("receiver:status file not found in gpscv.conf");

	if (list_get_string_value(last,"reference","status file",&stmp))
		refStatusFile=relativeToAbsolutePath(stmp,cvgpsHome);
	else
		log("reference:status file not found in gpscv.conf");

#ifdef MULTIRX
	// This is for V? of the NMIA TTS with GPS (NV08C/ublox9), GLONASS (SMT360), and BDS (NavSpark) receivers
	if (list_get_string_value(last,"GNSS","GLONASS status",&stmp))
	//if (list_get_string_value(last,"gnss","glonass status",&stmp))
		GLONASSStatusFile=relativeToAbsolutePath(stmp,cvgpsHome);
	else
		log("GLONASS status not found in gpscv.conf");
	
	if (list_get_string_value(last,"GNSS","Beidou status",&stmp))
	//if (list_get_string_value(last,"gnss","beidou status",&stmp))
		BeidouStatusFile=relativeToAbsolutePath(stmp,cvgpsHome);
	else
		log("Beidou status not found in gpscv.conf");
#endif
	
	list_clear(last);

	//
	// Parse sysmon.conf
	//

	if (!configfile_parse_as_list(&last,sysmonConfig.c_str())){
		ostringstream msg;
		msg << "failed to read " << sysmonConfig;
		log(msg.str());
		exit(EXIT_FAILURE);
	}

	if (list_get_string_value(last,"Alarms","Status File Directory",&stmp)){
		alarmPath=stmp;
		alarmPath+="/*";
	}
	else
		log("Status File Directory not found in sysmonitor.conf");

	list_clear(last);

}

void LCDMonitor::updateConfig(std::string section,std::string token,std::string val)
{
	
	DBGMSG(debugStream,TRACE, "Updating " << configFile);
	configfile_update(section.c_str(),token.c_str(),val.c_str(),configFile.c_str());
}

void LCDMonitor::log(std::string msg)
{
	FILE *fd;

	DBGMSG(debugStream,TRACE,msg);
	time_t tt =  time(0);
	struct tm *gmt = gmtime(&tt);
	char tc[128];

	strftime(tc,128,"%F %T",gmt);

	if ((fd = fopen(logFile.c_str(),"a"))){
		fprintf(fd,"%s %s\n",tc,msg.c_str());
		fclose(fd);
	}
}

void LCDMonitor::showHelp()
{
	cout << "Usage: lcdmonitor [options]" << endl;
	cout << "Available options are" << endl;
	cout << "\t-c <file>" << endl << "\t Use alternate configuration file" << endl;
	cout << "\t-d <file>" << endl << "\t Turn on debuggging" << endl;
	cout << "\t-h" << endl << "\t Show this help" << endl;
	cout << "\t-v" << endl << "\t Show version" << endl;
}

void LCDMonitor::showVersion()
{
	cout << "lcdmonitor v" << LCDMONITOR_VERSION << ", last modified " << LAST_MODIFIED << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

// Note that this is only ever made once
void LCDMonitor::makeMenu()
{
	menu = new Menu("Main menu");

	Menu *setupM = new Menu("Setup...");
	menu->insertItem(setupM);
	WidgetCallback<LCDMonitor> *cb;
	MenuItem *mi;

		networkM = new Menu("Networking ...");
		setupM->insertItem(networkM);
			LAN0M = new Menu("LAN1 ...");
			networkM->insertItem(LAN0M);
	
				cb = new WidgetCallback<LCDMonitor>(this,&LCDMonitor::networkConfigDHCP0);
				midDHCP0=LAN0M->insertItem("DHCP...",cb);
				mi = LAN0M->itemAt(midDHCP0);
				if (mi != NULL) mi->setChecked(addressAssignmentLAN[0]==DHCP);

				cb = new WidgetCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIP40);
				midStaticIP40=LAN0M->insertItem("Static IPv4...",cb);
				mi = LAN0M->itemAt(midStaticIP40);
				if (mi != NULL) mi->setChecked(addressAssignmentLAN[0]==Static);
				
		lcdSetup = new Menu("LCD setup...");
		setupM->insertItem(lcdSetup);
		
			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::LCDConfig);
			lcdSetup->insertItem("LCD settings...",cb);
			
			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::LCDBacklightTimeout);
			lcdSetup->insertItem("LCD backlight time..",cb);

		displayModeM = new Menu("Display mode...");
		setupM->insertItem(displayModeM);

			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::setGPSDisplayMode);
		  midGPSDisplayMode =  displayModeM ->insertItem("GPS",cb);
			mi = displayModeM->itemAt(midGPSDisplayMode);
			DBGMSG(debugStream,TRACE, "midGPSDisplayMode = " << midGPSDisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==GPS);

			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::setNTPDisplayMode);
		  midNTPDisplayMode = displayModeM ->insertItem("NTP",cb);
			mi = displayModeM->itemAt(midNTPDisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==NTP);

			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::setRefDisplayMode);
		  midGPSDODisplayMode = displayModeM ->insertItem("REF",cb);
			mi = displayModeM->itemAt(midGPSDODisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==REF);
#ifdef MULTIRX
			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::setGLOBDDisplayMode);
		  midGLOBDDisplayMode = displayModeM ->insertItem("GLOBD",cb);
			mi = displayModeM->itemAt(midGLOBDDisplayMode);
			DBGMSG(debugStream,TRACE,"midGLOBDDisplayMode = " << midGLOBDDisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==GLOBD);
#endif		
		cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::showIP);
		setupM->insertItem("Show IP addresses..",cb);

	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::showAlarms);
	menu->insertItem("Show alarms",cb);

	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::showSysInfo);
	menu->insertItem("Show system info",cb);

	Menu *restartM = new Menu("Restart...");
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::restartRx);
	restartM->insertItem("Restart GPS",cb);
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::restartNTP);
	restartM->insertItem("Restart NTP",cb);
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::reboot);
	restartM->insertItem("Reboot",cb);
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::poweroff);
	restartM->insertItem("Power down",cb);
	menu->insertItem(restartM);
}

void LCDMonitor::getResponse()
{
	int timed_out =1;
	int k;
	for(k=0;k<=300;k++){ // FIXME
		usleep(10000);
		if(packetReceived()){

			ShowReceivedPacket();
			timed_out = 0; 
			break;
		}
	}
	DBGMSG(debugStream,TRACE, k);
	if(timed_out){
		DBGMSG(debugStream,TRACE,"Timed out waiting for a response " );
		log("I/O timeout");

		Uninit_Serial();

		if(Serial_Init(PORT,BAUD)){
			DBGMSG(debugStream,TRACE, "Could not open port " << PORT << " at " << BAUD << " baud.");
			exit(EXIT_FAILURE);
		}
		else{
   	 DBGMSG(debugStream,TRACE, PORT << " opened at "<< BAUD <<" baud");
		}

		/* Clear the buffer */
		while(BytesAvail())
    	GetByte();
	}
}

void LCDMonitor::showCursor(bool on)
{
	outgoing_response.command = 12;
	if (on)
		outgoing_response.data[0]=4;
	else
		outgoing_response.data[0]=0;
	outgoing_response.data_length =1;
	send_packet();
	getResponse();
}

void LCDMonitor::updateLine(int row,std::string buf)
{
	// a bit of optimization to cut down on I/O and flicker
	if (buf==status[row]) return;
	status[row]=buf;
	std::string tmp=buf;
	if (buf.length() < 20) // pad out to clear the line
		tmp+=std::string(20-buf.length(),' ');

	outgoing_response.command = 31;
	outgoing_response.data[0]=0; //col
	outgoing_response.data[1]=row; //row
	int nch = tmp.length();
	if (nch > 20) nch=20;
	memcpy(&outgoing_response.data[2],tmp.c_str(),nch);
	outgoing_response.data_length =2+nch;
	send_packet();
	getResponse();
}

void LCDMonitor::updateStatusLED(int row,LEDState s)
{
	if (statusLED[row] == s) return;// nothing to do
	statusLED[row] = s;

	int idx = 12 - row*2;
	int rstate,gstate;
	switch (s)
	{
		case Off: rstate=gstate=0;break;
		case RedOn: rstate=100;gstate=0;break;
		case GreenOn: rstate=0;gstate=100;break;
		case Unknown:return;
	}

	outgoing_response.command = 34;
	outgoing_response.data[0]=idx;
	outgoing_response.data[1]=rstate;
	outgoing_response.data_length =2;
	send_packet();
	getResponse();

	outgoing_response.command = 34;
	outgoing_response.data[0]=idx-1;
	outgoing_response.data[1]=gstate;
	outgoing_response.data_length =2;
	send_packet();
	getResponse();
}

void LCDMonitor::statusLEDsOff()
{
	for (int i=0;i<4;i++)
		updateStatusLED(i,Off);
}

void LCDMonitor::updateCursor(int row,int col)
{
	outgoing_response.command = 11;
	outgoing_response.data[0]=col; // row
	outgoing_response.data[1]=row; //col
	outgoing_response.data_length =2;
	send_packet();
	getResponse();
}

void LCDMonitor::repaintWidget(Widget *w,std::vector<std::string> &display,bool forcePaint)
{

	if (w->dirty() || forcePaint){
		w->paint(display);

		for (int i=0;i<4;i++){
			if (display.at(i).size()>0)
					updateLine(i,display.at(i));
		}
	}
}

bool LCDMonitor::checkAlarms()
{
	alarms.clear();

	glob_t aglob;
	int globret=glob(alarmPath.c_str(),0,0,&aglob);
	if (globret == GLOB_NOMATCH)
	{
		globfree(&aglob);
		return true;
	}
	else if (globret == 0)
	{
		DBGMSG(debugStream,TRACE,"");
		for (unsigned int i=0;i<aglob.gl_pathc;i++)
		{
			// have to strip path
			std::string msg(aglob.gl_pathv[i]);
			size_t pos = msg.find_last_of('/');
			if (pos != string::npos)
				msg = msg.substr(pos+1,string::npos);
			alarms.push_back(msg);
			DBGMSG(debugStream,TRACE,msg);
		}
		globfree(&aglob);
		return false;
	}

	return false;
}

bool LCDMonitor::checkRx(int *nsats,std::string &prns,bool *unexpectedEOF)
{

	// TODO this should return a vector of PRNs rather than repeating parsing elsewhere 
	*unexpectedEOF=false;
	bool ret = checkFile(rxStatusFile.c_str());
	if (!ret)
	{
		DBGMSG(debugStream,TRACE, "stale file");
		return false; // don't display stale information
	}

	// status file is current so extract useful stuff
	std::ifstream fin(rxStatusFile.c_str());
	if (!fin.good()) // not really going to happen
	{
		DBGMSG(debugStream,TRACE,"stream error");
		return false;
	}

	std::string tmp;
	bool gotSats=false;

	while (!fin.eof()){
		getline(fin,tmp);
		if (string::npos != tmp.find("GPS sats")){
			gotSats=true;
			std::string sbuf;
			parseConfigEntry(tmp,sbuf,':');
			int ntmp=-1;
			ntmp=atoi(sbuf.c_str());
			if (ntmp >=0){
				gotSats=true;
				*nsats=ntmp;
			}
			break;
		}
		else if (string::npos != tmp.find("prns")){
			parseConfigEntry(tmp,prns,'=');
			//cout << "prns = " << prns << endl;
			break;
		}
		else if (tmp.rfind("GPS", 0) == 0){ // need to be careful because GPS can appear in tsys
			parseConfigEntry(tmp,prns,'=');
			std::vector<std::string> tmp;
			boost::split(tmp,prns,is_any_of(","));
			*nsats = tmp.size();
			gotSats = *nsats > 0;
			DBGMSG(debugStream,TRACE,prns << " " << *nsats);
			break;
		}
	}
	fin.close();
	*unexpectedEOF = !(gotSats || (!prns.empty()));
	DBGMSG(debugStream,TRACE,"done");
	return ret;
}

bool LCDMonitor::checkRef(std::string &status,std::string &ffe,std::string &EFC,std::string &health,bool *unexpectedEOF)
{

	*unexpectedEOF=false;
	bool ret = checkFile(refStatusFile.c_str());
	if (!ret){
		DBGMSG(debugStream,TRACE,"stale file");
		return false; // don't display stale information
	}

	// status file is current so extract useful stuff
	std::ifstream fin(refStatusFile.c_str());
	
	if (!fin.good()) // not really going to happen
	{
		DBGMSG(debugStream,TRACE,"stream error");
		return false;
	}

	std::string tmp;
	while (!fin.eof()){
		getline(fin,tmp);
		if (reference== LCXO){
			if (string::npos != tmp.find("Lock status                   : ")){
				parseConfigEntry(tmp,status,'-');
			}
			else if (string::npos != tmp.find("EFC percentage (%)            : ")){
				parseConfigEntry(tmp,EFC,':');
			}
			else if (string::npos != tmp.find("Estimated frequency accuracy  : ")){
				parseConfigEntry(tmp,ffe,':');
			}
			else if (string::npos != tmp.find("GPSDO health                  : ")){
				parseConfigEntry(tmp,health,':');
			}
		}
	
		else if (reference== ULN1100){
			if (string::npos != tmp.find("Reported precision ")){
				parseConfigEntry(tmp,status,'-');
			}
			else if (string::npos != tmp.find("EFC voltage: ")){
				parseConfigEntry(tmp,EFC,':');
			}
			else if (string::npos != tmp.find("OCXO frequency error estimate: ")){
				parseConfigEntry(tmp,ffe,':');
			}
			else if (string::npos != tmp.find("GPSDO health: ")){
				parseConfigEntry(tmp,health,':');
			}
		}
		else if (reference== Furuno){
			if (string::npos != tmp.find("Reported precision: ")){
				parseConfigEntry(tmp,status,':');
			}
			else if (string::npos != tmp.find("ffe: ")){
				parseConfigEntry(tmp,ffe,':');
			}
			else if (string::npos != tmp.find("efc: ")){
				parseConfigEntry(tmp,EFC,':');
			}
			else if (string::npos != tmp.find("Health: ")){
				parseConfigEntry(tmp,health,':');
			}
		}
	}
	
	fin.close();
	
	trim(status); // using boost
	trim(EFC);
	trim(ffe);
	trim(health);

	DBGMSG(debugStream,TRACE,"done");
	return ret;
}

#ifdef MULTIRX

bool LCDMonitor::checkGLOBD(std::string &GLOprns, std::string &BDprns, bool  *unexpectedEOF)
{
	*unexpectedEOF=false;
	bool ret = checkFile(GLONASSStatusFile.c_str());
	if (!ret){
		DBGMSG(debugStream,TRACE,"GLONASS stale file");
		return false; // don't display stale information
	}
	ret = checkFile(BeidouStatusFile.c_str());
	if (!ret){
		DBGMSG(debugStream,TRACE,"Beidou stale file");
		return false; // don't display stale information
	}
	// status files are current so extract useful stuff
	std::ifstream fin(GLONASSStatusFile.c_str());
	if (!fin.good()){ // not really going to happen
		DBGMSG(debugStream,TRACE,"GLONASS stream error");
		return false;
	}
	std::string tmp;
	while (!fin.eof()){
		getline(fin,tmp);
		if (string::npos != tmp.find("prns=")){
			parseConfigEntry(tmp,GLOprns,'=');
			
		}
	}
	fin.close();
	trim(GLOprns); // using boost
	// Now Beidou
	std::ifstream fin2(BeidouStatusFile.c_str());
	if (!fin2.good()){ // not really going to happen
		DBGMSG(debugStream,TRACE,"Beidou stream error");
		return false;
	}
	while (!fin2.eof()){
		getline(fin2,tmp);
		if (string::npos != tmp.find("prns=")){
			parseConfigEntry(tmp,BDprns,'=');
		}
	}
	fin2.close();
	trim(BDprns); // using boost
	*unexpectedEOF = ((GLOprns.empty()) || (BDprns.empty())) ;
	DBGMSG(debugStream,TRACE,"done");
	return ret;
}
#endif

bool LCDMonitor::detectNTPVersion()
{
	// ntpd versioning
	//
	// pre 4-2.2.
	// 	NTP uses A.B.C. - style release numbers.
	//
	// The third (C) part of the version number can be:
	//
	//    0-69 for releases on the A.B.C series.
	//    70-79 for alpha releases of the A.B+1.0 series.
	//    80+ for beta releases of the A.B+1.0 series.
	//
	// At the moment:
	//
	//    A is 4, for NTP version 4,
	//    B is the minor release number.
	//    C is the patch/bugfix number, and may have extra cruft in it.
	//
	// Any extra cruft in the C portion of the number indicates an "interim" release.
	// post 4.2.2
	// 	The syntax of a name is: Version[Point][Special][ReleaseCandidate]
	//
	// where Version is A.B.C, and:
	//
	//     * A is the protocol version (currently 4).
	//     * B is the major version number.
	//     * C is the minor version number. Even numbers are -stable releases, and odd numbers are -dev releases. 
	//
	// Point is the letter p followed by an increasing number.
	//
	// Special is currently only used for interim projects, and will generally be neither seen nor used by public releases.
	//
	// ReleaseCandidate is the string -RC.

	char buf[1024];
	bool ret=false;
	if (NTPDaemon == CHRONYD){
		FILE *fp=popen("/usr/bin/chronyc -v","r"); 
		while (fgets(buf,1023,fp) != NULL){
			DBGMSG(debugStream,TRACE, buf);
			boost::regex re("version\\s+(\\d+)\\.(\\d+)");
			boost::cmatch matches;
			if (boost::regex_match(buf,matches,re)){
				NTPProtocolVersion=boost::lexical_cast<int>(matches[1]);
				NTPCLIMajorVersion=boost::lexical_cast<int>(matches[1]);
				NTPCLIMinorVersion=boost::lexical_cast<int>(matches[2]);
				DBGMSG(debugStream,TRACE, "ver=" << NTPProtocolVersion << 
					",major=" << NTPCLIMajorVersion << ",minor=" << NTPCLIMinorVersion << endl);
			}
		}
		pclose(fp);
	}
	else if (NTPDaemon == NTPD){
		
		FILE *fp=popen("/usr/local/bin/ntpq -c version","r"); 
		while (fgets(buf,1023,fp) != NULL)
		{
			DBGMSG(debugStream,TRACE, buf);
			boost::regex re("^ntpq\\s+(\\d+)\\.(\\d+)\\.(\\d+).*");
			boost::cmatch matches;
			if (boost::regex_match(buf,matches,re)){
				NTPProtocolVersion=boost::lexical_cast<int>(matches[1]);
				NTPCLIMajorVersion=boost::lexical_cast<int>(matches[2]);
				NTPCLIMinorVersion=boost::lexical_cast<int>(matches[3]);
				DBGMSG(debugStream,TRACE, "ver=" << NTPProtocolVersion << 
					",major=" << NTPCLIMajorVersion << ",minor=" << NTPCLIMinorVersion << endl);
			}
		}
		pclose(fp);
	}
	return ret;
}

void LCDMonitor::getNTPstats(int *oldpkts,int *newpkts,int *badpkts)
{

	char buf[1024];

	if (NTPDaemon == CHRONYD){
		FILE *fp=popen("/usr/bin/chronyc serverstats","r");
		*oldpkts = 0;
		*badpkts = 0;
		while (fgets(buf,1023,fp) != NULL){
			DBGMSG(debugStream,TRACE, buf);
			if (strstr(buf,currPacketsTag.c_str())){
					char* sep = strchr(buf,':');
					if (sep!=NULL){
						if (strlen(sep) > 1){
							sep++;
							*newpkts=atoi(sep);
						}
					}
				}
		}
		pclose(fp);
	}
	else if (NTPDaemon == NTPD){
		// Louis 2016-10-25 ntpdc is deprecated, use ntpq now
		FILE *fp=popen("/usr/local/bin/ntpq -c sysstats","r");
		while (fgets(buf,1023,fp) != NULL){
			DBGMSG(debugStream,TRACE, buf);
			if (NTPProtocolVersion == 4){

				if (strstr(buf,currPacketsTag.c_str())){
					char* sep = strchr(buf,':');
					if (sep!=NULL){
						if (strlen(sep) > 1){
							sep++;
							*newpkts=atoi(sep);
						}
					}
				}
				else if(strstr(buf,oldPacketsTag.c_str())){
					char* sep = strchr(buf,':');
					if (sep!=NULL){
						if (strlen(sep) > 1){
							sep++;
							*oldpkts=atoi(sep);
						}
					}
				}
				else if(strstr(buf,badPacketsTag.c_str())){
					char* sep = strchr(buf,':');
					if (sep!=NULL){
						if (strlen(sep) > 1){
							sep++;
							*badpkts=atoi(sep);
							//printf("For badpkts:\n%s",buf);
						}
					}
				}
			}
		}
		pclose(fp);
	}
	DBGMSG(debugStream,TRACE, "old,new,bad =" << *oldpkts << " " << *newpkts << " " << *badpkts);
}

bool LCDMonitor::checkFile(const char *fname)
{
	struct stat statbuf;
	time_t ttime;
	struct sysinfo info;
	int retval=0;

	time(&ttime);
	sysinfo(&info);
	retval = stat(fname,&statbuf);

	if (retval == 0){ /* file exists */
		/* Was it created since the last boot */
		if (statbuf.st_mtime > ttime - info.uptime){
			/* Is it current ? */
			if (ttime - statbuf.st_mtime < MAX_FILE_AGE){
				DBGMSG(debugStream,TRACE,  fname << " ok");
				return true;
			}
			else{
				DBGMSG(debugStream,TRACE, fname <<" too old" << ttime - statbuf.st_mtime);
				return false;
			}
		}
		else{
			DBGMSG(debugStream,TRACE,  fname <<" predates boot" << statbuf.st_mtime << " > " << ttime - info.uptime);
			return false; /* predates boot */
		}

	}
	else{ /* file doesn't exist */
		/* Have we just booted ? OK if we have */
		DBGMSG(debugStream,TRACE,  fname << "doesn't exist");
		return (info.uptime < BOOT_GRACE_PERIOD);
	}

	return (retval != 0);
}

bool LCDMonitor::serviceEnabled(const char *service)
{
	return true;
}

bool LCDMonitor::runSystemCommand(std::string cmd,std::string okmsg,std::string failmsg)
{
	int sysret = system(cmd.c_str());
	DBGMSG(debugStream,TRACE,   cmd << " returns " << sysret);
	if (sysret == 0){
		log(okmsg);
		updateLine(2,okmsg);
	}
	else{
		log(failmsg);
		updateLine(2,failmsg);
	}
	sleep(2);
	return (sysret==0);
}

bool LCDMonitor::runCommand(std::string cmd,std::vector<std::string> &output)
{
	char buf[1024];
	bool ret=true;
	FILE *fp=popen(cmd.c_str(),"r"); 
		
	if (NULL==fp){
		return false;
	}
	
	while (fgets(buf,1023,fp) != NULL){
		DBGMSG(debugStream,TRACE, buf);
		output.push_back(buf);
	}
	pclose(fp);
	return ret;
}
		

string LCDMonitor::relativeToAbsolutePath(string path,string rootDir)
{
	string absPath=path;
	if (path.size() > 0){
		if (path.at(0) == '/')
			absPath = path;
		else
			absPath=rootDir+path;
	}
	return absPath;
}

void LCDMonitor::parseConfigEntry(std::string &entry,std::string &val,char delim)
{
	size_t pos = entry.find(delim);
	if (pos != string::npos){
		val = entry.substr(pos+1);
		// Strip any leading or trailing quotes
		size_t pos = val.find_first_of('"');
		if (pos != string::npos)
			val = val.substr(pos+1);
		pos = val.find_last_of('"');
		if (pos != string::npos)
			val = val.substr(0,pos);
	}
}

std::string  LCDMonitor::prefix2netmask(std::string pfx)
{
	int cidr = atoi(pfx.c_str());
	uint32_t ipv4nm = 0xffffffff;
	ipv4nm  <<=  32-cidr; // bytes are in host order
	char buf[INET_ADDRSTRLEN+1];
	struct in_addr sin_addr;
	sin_addr.s_addr= htonl(ipv4nm); // to network byte order
	inet_ntop(AF_INET,&sin_addr,buf,INET_ADDRSTRLEN + 1);
	DBGMSG(debugStream,TRACE,"netmask " << buf);
	return std::string(buf);
}

std::string  LCDMonitor::netmask2prefix(std::string nm)
{
	struct in_addr sin_addr;
	inet_pton(AF_INET,nm.c_str(),&sin_addr);
	uint32_t ipv4nm = ntohl(sin_addr.s_addr); // to host byte order
	int nbits=0;
	ipv4nm = ~ipv4nm;
	while (ipv4nm != 0){
		ipv4nm >>= 1;
		nbits++;
	}
	DBGMSG(debugStream,TRACE,"prefix " << 32 - nbits);
	std::string ret = boost::lexical_cast<std::string>(32 - nbits);
	return ret;
}

std::string LCDMonitor::quote(std::string s)
{
	return "\"" + s + "\"";
}

		
