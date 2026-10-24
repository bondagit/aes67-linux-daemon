// basic file operations
#include <iostream>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <vector>

using namespace std;

static void hexdump_bytes(ifstream &fl, streamoff pos, size_t n)
{
  vector<uint8_t> buf(n);
  fl.clear();
  fl.seekg(pos);
  fl.read(reinterpret_cast<char*>(buf.data()), n);
  size_t r = fl.gcount();
  for (size_t i = 0; i < r; ++i) {
    if (i) cout << " ";
    cout << hex << setw(2) << setfill('0') << (int)buf[i];
  }
  cout << dec << "\n";
}

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
  if (channels % 2 != 0 || channels <= 0 || channels > 64) {
    cerr << "Unsupported channels " << channels << endl;
    exit(1);
  }

  ifstream fl;
  fl.open("./sink_test.raw", ios::in|ios::binary);
  if (!fl.is_open()) {
    cout << "cannot open file " << endl;
    exit(1);
  }
  fl.seekg(0, ios::end);
  auto end = fl.tellg();
  fl.seekg(0, ios::beg);
  auto prev = fl.tellg();
  // skip initial silence (zero bytes)
  while ((prev = fl.tellg()) != end) {
    int c = fl.get();
    if (c != 0 && c != EOF) {
      fl.unget();
      break;
    }
    if (c == EOF) break;
  }

  uint8_t curr = 0, byte = 0;
  bool first = true;
  fl.seekg(prev);
  bool mismatch = false;
  streamoff mismatch_pos = -1;
  while (fl.tellg() != end && !mismatch) {
    int ch(channels);
    while (ch--) {
      if (!first) {
        int c = fl.get();
        if (c == EOF) { mismatch = true; break; }
        byte = (uint8_t)c;
        if (byte != (uint8_t)curr) {
          mismatch = true;
          auto p = fl.tellg();
          mismatch_pos = static_cast<std::streamoff>(p) - 1;
          break;
        }
      }
      if (len > 1) {
        int c = fl.get(); if (c == EOF) { mismatch = true; break; }
        byte = (uint8_t)c;
        if (byte != (uint8_t)(curr+1)) {
          mismatch = true; auto p = fl.tellg(); mismatch_pos = static_cast<std::streamoff>(p) - 1; break;
        }
        if (len > 2) {
          c = fl.get(); if (c == EOF) { mismatch = true; break; }
          byte = (uint8_t)c;
          if (byte != (uint8_t)(curr+2)) {
            mismatch = true; auto p = fl.tellg(); mismatch_pos = static_cast<std::streamoff>(p) - 1; break;
          }
          if (len > 3) {
            c = fl.get(); if (c == EOF) { mismatch = true; break; }
            byte = (uint8_t)c;
            if (byte != (uint8_t)(curr+3)) { mismatch = true; auto p = fl.tellg(); mismatch_pos = static_cast<std::streamoff>(p) - 1; break; }
          }
        }
      }
      first = false;
    }
    curr += len;
  }

  int rc = 0;
  if (first || mismatch || fl.tellg() != end) {
    if (mismatch_pos == -1) mismatch_pos = fl.tellg();
    cout << "error at position: " << mismatch_pos << endl;
    cout << "Expected bytes (hex): ";
    // expected sequence begins at curr - len (we incremented after last sample)
    streamoff exp_pos = mismatch_pos;
    // compute expected base value for the sample containing the mismatch
    // we cannot easily map pos->sample without extra math; instead print expected pattern based on curr
    // print expected bytes for one sample
    uint8_t expected_base = curr;
    cout << hex << nouppercase << setfill('0');
    for (int i = 0; i < len; ++i) {
      if (i) cout << " ";
      cout << setw(2) << (int)((uint8_t)(expected_base + i));
    }
    cout << dec << "\n";
    cout << "Actual bytes at file (hex): ";
    hexdump_bytes(fl, mismatch_pos, max(8, len*2));
    rc = 1;
  } else {
    cout << "ok" << endl;
  }
  fl.close();
  return rc;
}
