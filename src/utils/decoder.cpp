#include <iostream>
#include <fstream>
#include <lzma.h>
#include <string>
#include <sstream>

#include "decoder.hpp"

using namespace std;

/**
 * @brief Prints an error message to stderr.
 * @param ret The error code returned by the liblzma function.
 */
static void print_error_message(lzma_ret ret)
{
  cerr << "Error: decompression failed\n";
  // print details
  switch (ret) {
    case LZMA_NO_CHECK:
      cerr << "Input file does not have a valid integrity check\n";
      break;
    case LZMA_UNSUPPORTED_CHECK:
      cerr << "Cannot calculate the integrity check\n";
      break;
    case LZMA_GET_CHECK:
      cerr << "Integrity check type is now available\n";
      break;
    case LZMA_MEM_ERROR:
      cerr << "Memory allocation failed\n";
      break;
    case LZMA_MEMLIMIT_ERROR:
      cerr << "Memory usage limit was reached\n";
      break;
    case LZMA_FORMAT_ERROR:
      cerr << "File format not recognized\n";
      break;
    case LZMA_OPTIONS_ERROR:
      cerr << "Invalid options\n";
      break;
    case LZMA_DATA_ERROR:
      cerr << "Data is corrupt\n";
      break;
    case LZMA_BUF_ERROR:
      cerr << "No progress is possible\n";
      break;
    case LZMA_PROG_ERROR:
      cerr << "Programming error\n";
      break;
    default:
      cerr << "Unknown error\n";
      break;
  }
}

/**
 * @brief Converts a number of bytes to a human-readable string.
 * @param n_bytes The number of bytes.
 * @return A string representing the number of bytes in a human-readable format.
 */
static string print_size(uint64_t n_bytes) {
  string s;
  s = to_string(n_bytes % 1024) + "B";
  n_bytes /= 1024;
  if (n_bytes == 0)
    return s;
  s = to_string(n_bytes % 1024) + "KB " + s;
  n_bytes /= 1024;
  if (n_bytes == 0)
    return s;
  s = to_string(n_bytes % 1024) + "MB " + s;
  n_bytes /= 1024;
  if (n_bytes == 0)
    return s;
  s = to_string(n_bytes % 1024) + "GB " + s;
  n_bytes /= 1024;
  if (n_bytes == 0)
    return s;
  s = to_string(n_bytes) + "TB " + s;
  return s;
}

bool decompress_xz(const char* filename, ostringstream& output)
{
  lzma_stream strm = LZMA_STREAM_INIT;
  lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
  if (ret != LZMA_OK) {
    cerr << "Error: failed to initialize the decompression stream\n";
    return false;
  }
  ifstream file(filename, ios::binary);
  if (!file.is_open()) {
    cerr << "Error: could not open file " << filename << endl;
    return false;
  }
  unsigned const buff_size = 256;
  char buffer_in[buff_size];
  char buffer_out[buff_size];

  do {
    strm.avail_in = file.readsome(buffer_in, sizeof(buffer_in));
    if (strm.avail_in == 0)
      break;
    strm.next_in = reinterpret_cast<const uint8_t*>(buffer_in);
    do {
      strm.avail_out = sizeof(buffer_out);
      strm.next_out = reinterpret_cast<uint8_t*>(buffer_out);
      ret = lzma_code(&strm, LZMA_RUN);
      if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
        print_error_message(ret);
        return false;
      }
      output.write(buffer_out, sizeof(buffer_out) - strm.avail_out);
    } while (strm.avail_out == 0);
  } while (ret != LZMA_STREAM_END);
  cout << "Total Decompressed " << print_size(strm.total_in) << " to " << print_size(strm.total_out) << endl;

  return true;
}
