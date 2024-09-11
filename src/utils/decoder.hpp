/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/utils/decoder.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the definition of the XZ
 * decompression function.
 */
#pragma once

#include <lzma.h>
#include <fstream>
#include <string>

/**
 * @brief Decompresses a XZ file and writes the output to a stream.
 * @param filename The name of the file to decompress.
 * @param output The stream to write the output to.
 * @return true if the decompression was successful, false otherwise.
 * @details This function uses the liblzma library to decompress the file.
 * @details If the decompression fails, an error message is printed to stderr.
 */
bool decompress_xz(const char* filename, std::ostringstream& output);
