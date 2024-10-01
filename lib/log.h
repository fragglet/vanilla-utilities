//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

void SetLogDistinguisher(const char *name);
void LogMessage(const char *fmt, ...);
void Error(const char *error, ...);
void ErrorPrintUsage(const char *fmt, ...);
void CheckAbort(const char *operation);

