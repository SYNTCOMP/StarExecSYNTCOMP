/*
 * File:  ReadException.hh
 * Author:  mikolas
 * Created on:  Fri Sep 2 19:43:25 GMTDT 2011
 * Copyright (C) 2011, Mikolas Janota
 */
#ifndef READEXCEPTION_HH_20477
#define READEXCEPTION_HH_20477
#include <string>
#include <exception>
using std::exception;
using std::string;
class ReadException : public exception {
public:
    ReadException(const string& message);
    ~ReadException()  throw() { delete[] s; }
    const char* what() const throw();
private:
    char* s;
};
#endif /* READEXCEPTION_HH_20477 */
