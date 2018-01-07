#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace std;

struct MemoryRegion
{
  void* startAddr;
  void* endAddr;
  int isReadable;
  int isWriteable;
  int isExecutabl;
};

int
main(int argc, char** argv)
{
  int pid;
  if (argc == 2) {
    pid = stoi(string(argv[1]));
  } else {
    pid = ::getpid();
  }

  ifstream fin("/proc/" + to_string(pid) + "/maps");
  ofstream fout(to_string(pid) + ".maps");

  string line;
  MemoryRegion mr;
  long total_read_only, total_read_write;

  while (fin) {
    getline(fin, line);
    char r, w, x, p;
    sscanf(line.c_str(),
           "%p-%p %c%c%c%c",
           &mr.startAddr,
           &mr.endAddr,
           &r,
           &w,
           &x,
           &p);

    mr.isReadable = r == 'r';
    mr.isWriteable = w == 'w';
    mr.isExecutabl = x == 'x';

    fout << mr.startAddr << ' ' << mr.endAddr << ' ' << mr.isReadable << ' '
         << mr.isWriteable << ' ' << mr.isExecutabl << endl;

    if (mr.isReadable and not mr.isWriteable) {
      total_read_only = (long)mr.endAddr - (long)mr.startAddr;
    } else if (mr.isReadable and mr.isWriteable) {
      total_read_write = (long)mr.endAddr - (long)mr.startAddr;
    }
  }

  cout << "PID: " << pid << endl;
  cout << "Total Read Only Memory: " << total_read_only << endl;
  cout << "Total Read Write Memory: " << total_read_write << endl;
}
