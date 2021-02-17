// basic file operations
#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char* argv[]) 
{
  if (argc < 3) {
    cerr << "Usage: " << argv[0] << " sample_format channels" << endl;
    exit(1);
  }

  int len(0);
  string format(argv[1]);
  if (format == "S16_LE")
    len = 2;
  else if (format == "S24_3LE")
    len = 3;
  else if (format == "S32_LE")
    len = 4;
  else {
    cerr << "Unsupported format " << format << endl;
    exit(1);
  }

  int channels = atoi(argv[2]);
  if (channels != 1 && channels != 2 && channels != 4 && channels != 8) {
    cerr << "Unsupported channels " << channels << endl;
    exit(1);
  }

  fstream fl;
  fl.open("./sink_test.raw", ios::in|ios::binary);
  if (!fl.is_open()) {
    cout << "cannot open file " << endl;
    exit(1);
  }
  auto begin = fl.tellp();
  fl.seekp(0, ios::end);
  auto end = fl.tellp();
  fl.seekp(0, ios::beg);
  auto prev = fl.tellp();
  // skip inital silence
  while((prev = fl.tellp()) != end && fl.get() == 0);

  uint8_t curr = 0, byte = 0;
  bool first = true;
  fl.seekp(prev);
  while(fl.tellp() != end) {
    int ch(channels);
    while (ch--) {
      if (!first) {
        byte = fl.get();
        if (byte != (uint8_t)curr) goto end;
      }
      if (len > 1) {
        byte = fl.get();
        if (byte != (uint8_t)(curr+1)) goto end;
        if (len > 2) {
          byte = fl.get();
          if (byte != (uint8_t)(curr+2)) goto end;
          if (len > 3) {
            byte = fl.get();
            if (byte != (uint8_t)(curr+3)) goto end;
	  }
        }
      }
      first = false;
    }
    curr += len;
  }

end:
  //cout << "expected " << (int)curr << " byte " << (int)byte << endl;
  int rc = 0;
  if (first || fl.tellp() != end)
  {
     cout << "error at position: " << fl.tellp() << endl;
     rc = 1;
  }
  else cout << "ok" << endl;
  fl.close();
  return rc;
}
