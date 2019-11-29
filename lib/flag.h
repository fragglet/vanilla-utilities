void BoolFlag(const char *name, int *ptr, const char *help_text);
void IntFlag(const char *name, int *ptr,
             const char *param_name, const char *help_text);
void StringFlag(const char *name, char **ptr,
             const char *param_name, const char *help_text);
void ParseCommandLine(int *argc, char ***argv);
