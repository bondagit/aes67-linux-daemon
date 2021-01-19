// basic file operations
#include <iostream>
#include <fstream>

using namespace std;

int main () 
{
  fstream fl;
  fl.open("./sink_test.wav", ios::in|ios::binary);
  if (!fl.is_open()) {
    cout << "cannot open file " << endl;
    exit(1);
  }
  auto begin = fl.tellp();
  fl.seekp(0, ios::end);
  auto end = fl.tellp();
  fl.seekp(68, ios::beg);
  unsigned char ch = 0;
  auto prev = fl.tellp();
  while(fl.tellp() < end && ch == 0)
  {
    prev = fl.tellp();
    ch = fl.get();
  }
  unsigned char curr = 0;
  bool first = true;
  fl.seekp(prev);
  while(fl.tellp() < end)
  {
    if (!first) {
      ch = fl.get();
      if (ch != (uint8_t)curr) break;
    }
    ch = fl.get();
    if (ch != (uint8_t)(curr+1)) break;
    ch = fl.get();
    if (ch != (uint8_t)(curr+2)) break;
    ch = fl.get();
    if (ch != (uint8_t)curr) break;
    ch = fl.get();
    if (ch != (uint8_t)(curr+1)) break;
    ch = fl.get();
    if (ch != (uint8_t)(curr+2)) break;
    curr += 3;
    first = false;
  }
  int rc = 0;
  if (fl.tellp() != end)
  {
     cout << "error at position: " << fl.tellp() << endl;
     rc = 1;
  }
  else cout << "ok" << endl;
  fl.close();
  return rc;
}
