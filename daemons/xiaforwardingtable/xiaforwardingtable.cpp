#include "xiaforwardingtable.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
//#include <unordered_map>
//#include <iomanip>

using namespace std;

void add_to_table(unordered_map<string, RouterEntry>& router_table, RouterEntry entry) {
    if (lookup_Route(router_table, entry.xid) != 0) {
    	router_table[entry.xid] = entry;
    } else {
	    cout<<"\nReplacing RouteData for exiting XID: "<<(entry.xid).c_str()<<endl;
	    auto _it = router_table.find(entry.xid);
	    _it->second = entry; //just replace with the new routeData for existing xid
    }  
}

void delete_from_table(unordered_map<string, RouterEntry>& router_table, string xid) {
    router_table.erase(xid);
}


/**
 * Print the current routes hashtable*
 * @param routerEntry hashtable
 * @return void
 **/
void display_table(const unordered_map<string, RouterEntry> &router_table) {

  cout << left << setw(8) <<"TYPE"<<left<<setw(45)<<"XID"<<left<<setw(8)<<"PORT"<<left<<setw(13)
            <<" NEXT HOP" <<left<<setw(8)<<"FLAG"<<endl;

  for(const auto& keyVal : router_table) {
    cout << left <<setw(8)<<(keyVal.second).type <<left<<setw(45) << keyVal.first
             << left <<setw(8) << (keyVal.second).port <<left<< setw(20)<<(keyVal.second).nextHop
             << right<<setw(8)<< hex <<setfill('0')<< atoi(((keyVal.second).flags).c_str())<<setfill(' ') << endl; 
  }
}

 /**
 * Retrieve the route entry in routerEntry hashtable
 * @param routerEntry hashtable 
 * @param  - XID
 * @return  - iterator of the located routeEntry, NULL if not found in hashtable
 **/

 int lookup_Route(unordered_map<string, RouterEntry>& _rt, string xid ) {
	unordered_map<string, RouterEntry>::iterator itr = _rt.find(xid);
	int retVal =0;
	if (itr != _rt.end()) {
		cout<<"\nRouteEntry is existing already! " <<itr->first<<endl;
	} else {
		cout<<"\nRouteEntry is not found. Adding to routetable ..."<<endl;
		retVal= -1;
	}
	return retVal;
}

void write_table_to_file(string fpath, unordered_map<string, RouterEntry>& router_table) {
    string fname = fpath + "xiaforwardingdata.csv";
    ofstream file(fname);
    if (file.is_open()) {
        for (unordered_map<string, RouterEntry>::iterator it = router_table.begin(); it != router_table.end(); ++it) {
            file << it->second.type << "," << it->second.xid << "," << it->second.port << "," 
		    <<it->second.nextHop<<","<< it->second.flags << "\n";
        }
        file.close();
    }
    else {
        cerr << "Error: Could not open file " << fname.c_str()<< " for writing" << endl;
    }
}

void read_table_from_file(string fpath, unordered_map<string, RouterEntry>& router_table) {
    string fname = fpath + "xiaforwardingdata.csv";
    FILE *cf;
    ifstream file(fname.c_str());
	
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            stringstream ss(line);
            string temp;

            RouterEntry entry;
            getline(ss, entry.type, ',');
            getline(ss, entry.xid, ',');
            getline(ss, entry.port, ',');
	    getline(ss, entry.nextHop, ',');
            getline(ss, entry.flags, ',');
	    printf("%s", line.c_str());

            add_to_table(router_table, entry);
        }
        file.close();
    } else {
	//will create an empty file if not existing
        cf = fopen(fname.c_str(), "w");
        if (cf != NULL) {
		cout<<"The empty forwardingtable file is created!"<<endl;
        } else {
  		cerr << "Error: Could not open file " << fname.c_str() << " for reading" << endl;
    		}
	}
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        cout << "Error: No input string provided!" << endl;
        return 1;
    }

    string arg1 = argv[1];
    string input_string = argv[3];
    string path = argv[2];
    stringstream ss(input_string);
    string temp;

    // Create a hash table for router entries
    unordered_map<string, RouterEntry> router_table;

    // Read the existing entries from file if arg1 is "-a"
    if (!path.empty() && (arg1 == "-a" || arg1 == "-r")) {
        read_table_from_file(path, router_table);
    }

    // Parse the input string and update the router table
    while (getline(ss, temp, ',')) {
        RouterEntry entry;
        entry.type = temp;
        getline(ss, entry.xid, ',');
        getline(ss, entry.port, ',');
	getline(ss, entry.nextHop, ',');
        getline(ss, entry.flags, ',');

        if (arg1 == "-a") {
            add_to_table(router_table, entry);
        } else if (arg1 == "-r") {
            delete_from_table(router_table, entry.xid);
        }
    }

    // Write the updated table to file if arg1 is "-a" or "-r"
    if (arg1 == "-a" || arg1 == "-r") {
        write_table_to_file(path, router_table);
    }

    cout<<"print out the current routertable!!!!"<<endl;
    display_table(router_table);

    return 0;
}
