
enum uf2_result_e {
  UF2_LOADED = 0,
  UF2_WRONG_PLATFORM,
  UF2_BAD,
  UF2_UNKNOWN,
};


enum uf2_result_e load_application_from_uf2(const char *filename);

