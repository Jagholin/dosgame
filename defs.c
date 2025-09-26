/*
    gmm2json: program that reads Gridmonger's GMM file and converts it into
   JSON format
    Copyright (C) 2025 Jagholin (github.com/Jagholin)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see
   <https://www.gnu.org/licenses/>
*/
#include "defs.h"

const RESULT RES_OK = 0;
const RESULT RES_ERR = -1;
const RESULT RES_BUFFER_TOO_SMALL = -2;
const RESULT RES_BAD_INPUT = -3;

// const char oom_message[] = "Out of memory.\n\r";
RESULT last_error = RES_OK;
