struct hack {
	char *desc;
	unsigned int version;
	std::vector<unsigned int> offset;
	std::vector<unsigned int> value;
};

u32 LoadHacks( void );
