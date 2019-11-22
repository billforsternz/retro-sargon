void translate_init();

// Return true if translated    
bool translate_z80( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, bool hybrid, std::string &out );

// Return true if translated    
bool translate_x86( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, std::set<std::string> &labels, std::string &out );

