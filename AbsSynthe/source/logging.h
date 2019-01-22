/**************************************************************************
 * Copyright (c) 2015, Guillermo A. Perez, Universite Libre de Bruxelles
 * 
 * This file is part of the (Swiss) AbsSynthe tool.
 * 
 * AbsSynthe is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * AbsSynthe is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with AbsSynthe.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * 
 * Guillermo A. Perez
 * Universite Libre de Bruxelles
 * gperezme@ulb.ac.be
 *************************************************************************/

#ifndef ABSSYNTHE_LOGGING_H
#define ABSSYNTHE_LOGGING_H

#include <string>

void parseLogLevelString(const char*);
void dbgMsg(const char*);
void wrnMsg(const char*);
void logMsg(const char*);
void errMsg(const char*,int code=1);
void dbgMsg(std::string);
void wrnMsg(std::string);
void logMsg(std::string);
void errMsg(std::string,int code=1);
void resetTimer(std::string);
void addTime(std::string);
clock_t getAccTime(std::string);

#endif
