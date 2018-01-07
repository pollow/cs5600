#include <fstream>
#include <iostream>
#include <string>

using namespace std;

int
main(int argc, char** argv)
{
  int pid;
  if (argc == 2) {
    pid = stoi(string(argv[1]));
  } else {
    cerr << "PID is needed." << endl;
    return 1;
  }

  ifstream fin(to_string(pid) + ".maps");
  long total_read_only, total_read_write;

  while (fin) {
    void *startAddr, *endAddr;
    bool r, w, x;
    fin >> startAddr >> endAddr >> r >> w >> x;
    cout << startAddr << '-' << endAddr << ' ';
    cout << "flags: ";
    if (r) {
      cout << "Readable ";
    }
    if (w) {
      cout << "Writable ";
    }
    if (x) {
      cout << "Executable";
    }
    cout << endl;

    if (r and not w) {
      total_read_only = (long)endAddr - (long)startAddr;
    } else if (r and w) {
      total_read_write = (long)endAddr - (long)startAddr;
    }
  }

  cout << "PID: " << pid << endl;
  cout << "Total Read Only Memory: " << total_read_only << endl;
  cout << "Total Read Write Memory: " << total_read_write << endl;

  return 0;
}
