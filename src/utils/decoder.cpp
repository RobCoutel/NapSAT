/**
 * @file src/utils/decoder.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the implementation of the XZ
 * decompression function.
 */

#include <iostream>
#include <fstream>
#include <lzma.h>
#include <string>
#include <sstream>

#include "../include/SAT-types.hpp"
#include "decoder.hpp"

using namespace std;

/**
 * @brief Prints an error message to stderr.
 * @param ret The error code returned by the liblzma function.
 */
static void print_error_message(lzma_ret ret)
{
  LOG_ERROR("decompression failed");
  // print details
  switch (ret) {
    case LZMA_NO_CHECK:
      LOG_ERROR("Input file does not have a valid integrity check");
      break;
    case LZMA_UNSUPPORTED_CHECK:
      LOG_ERROR("Cannot calculate the integrity check");
      break;
    case LZMA_GET_CHECK:
      LOG_ERROR("Integrity check type is now available");
      break;
    case LZMA_MEM_ERROR:
      LOG_ERROR("Memory allocation failed");
      break;
    case LZMA_MEMLIMIT_ERROR:
      LOG_ERROR("Memory usage limit was reached");
      break;
    case LZMA_FORMAT_ERROR:
      LOG_ERROR("File format not recognized");
      break;
    case LZMA_OPTIONS_ERROR:
      LOG_ERROR("Invalid options");
      break;
    case LZMA_DATA_ERROR:
      LOG_ERROR("Data is corrupt");
      break;
    case LZMA_BUF_ERROR:
      LOG_ERROR("No progress is possible");
      break;
    case LZMA_PROG_ERROR:
      LOG_ERROR("Programming error");
      break;
    default:
      LOG_ERROR("Unknown error");
      break;
  }
}

/**
 * @brief Converts a number of bytes to a human-readable string.
 * @param n_bytes The number of bytes.
 * @return A string representing the number of bytes in a human-readable format.
 */
static string byte_size_to_string(uint64_t n_bytes) {
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
    LOG_ERROR("failed to initialize the decompression stream");
    return false;
  }
  ifstream file(filename, ios::binary);
  if (!file.is_open()) {
    LOG_ERROR("could not open file " << filename);
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
  LOG_INFO("Total Decompressed " << byte_size_to_string(strm.total_in) << " to " << byte_size_to_string(strm.total_out));

  return true;
}
