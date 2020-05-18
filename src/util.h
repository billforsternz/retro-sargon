/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: util.h
 *       Simple C++ utility functions
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <iostream>
#include <string>
#include <vector>

namespace util
{
void putline(std::ostream &out,const std::string &line);
std::string sprintf( const char *fmt, ... );
bool prefix( const std::string &s, const std::string prefix );
bool suffix( const std::string &s, const std::string suffix );
bool prefix_remove( std::string &s, const std::string prefix );
void ltrim( std::string &s );
void rtrim( std::string &s );
bool trim( std::string &s );
void tests();
void replace_all( std::string &s, const std::string from, const std::string to );
void replace_once( std::string &s, const std::string from, const std::string to );
void split( const std::string &s, std::vector<std::string> &fields );
std::string toupper( const std::string &s );
std::string tolower( const std::string &s );
}

#endif // UTIL_H_INCLUDED

