#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <iomanip>

using namespace std;

struct RouterEntry {
    string type;
    string xid;
    string port;
    string next_hop;
};

void add_to_table(unordered_map<string, RouterEntry>& router_table, RouterEntry entry) {
    router_table[entry.xid] = entry;
}

void delete_from_table(unordered_map<string, RouterEntry>& router_table, string xid) {
    router_table.erase(xid);
}

void list_table(unordered_map<string, RouterEntry>& router_table) {
    // Print table headers
    cout << "Type\tXID\tPort\tNext Hop" << endl;

    // Iterate over entries and print them out in a table format
    for (unordered_map<string, RouterEntry>::iterator it = router_table.begin(); it != router_table.end(); ++it) {
        cout << it->second.type << "\t" << it->first << "\t" << it->second.port << "\t" << it->second.next_hop << endl;
    }
}

void write_table_to_file(string filename, unordered_map<string, RouterEntry>& router_table) {
    ofstream file(filename);
    if (file.is_open()) {
        for (unordered_map<string, RouterEntry>::iterator it = router_table.begin(); it != router_table.end(); ++it) {
            file << it->second.type << "," << it->second.xid << "," << it->second.port << "," << it->second.next_hop << "\n";
        }
        file.close();
        cout << "Router table written to file " << filename << endl;
    }
    else {
        cerr << "Error: Could not open file " << filename << " for writing" << endl;
    }
}

void read_table_from_file(string filename, unordered_map<string, RouterEntry>& router_table) {
    ifstream file(filename);
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            stringstream ss(line);
            string temp;

            RouterEntry entry;
            getline(ss, entry.type, ',');
            getline(ss, entry.xid, ',');
            getline(ss, entry.port, ',');
            getline(ss, entry.next_hop, ',');

            add_to_table(router_table, entry);
        }
        file.close();
        cout << "Router table read from file " << filename << endl;
    }
    else {
        cerr << "Error: Could not open file " << filename << " for reading" << endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cout << "Error: No input string provided!" << endl;
        return 1;
    }

    string arg1 = argv[1];
    string input_string = argv[2];
    stringstream ss(input_string);
    string temp;

    // Create a hash table for router entries
    unordered_map<string, RouterEntry> router_table;

    // Read the existing entries from file if arg1 is "-a"
    if (arg1 == "-a" || arg1 == "-r") {
        read_table_from_file("router_table.txt", router_table);
    }

    // Parse the input string and update the router table
    while (getline(ss, temp, ',')) {
        RouterEntry entry;
        entry.type = temp;
        getline(ss, entry.xid, ',');
        getline(ss, entry.port, ',');
        getline(ss, entry.next_hop, ',');

        if (arg1 == "-a") {
            add_to_table(router_table, entry);
        } else if (arg1 == "-r") {
            delete_from_table(router_table, entry.xid);
        }
    }

    // Write the updated table to file if arg1 is "-a" or "-r"
    if (arg1 == "-a" || arg1 == "-r") {
        write_table_to_file("router_table.txt",router_table);
    }

    // Print out the contents of the router table
    list_table(router_table);

    return 0;
}