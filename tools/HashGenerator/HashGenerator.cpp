// HashGenerator.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory>
#include "../../src/Shared/sha1.h"
#include "../../src/Shared/version.h"

#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#define SwapEndian(x) _byteswap_ulong(x)
#else
#define SwapEndian(x) __builtin_bswap32(x)
#include <unistd.h>
#include <string.h>
#endif

typedef struct {
	unsigned int prod_version;
	unsigned prod_sha1_hash[5];
	unsigned int rc_version;
	unsigned int rc_number;
	unsigned rc_sha1_hash[5];
} UpdateStructV1;

void Display_Parameters(void)
{
	printf("parameters:\n");
	printf("HashGenerator [stable_dol_path] stable_version [rc_dol_path] rc_version rc_number\n\n");
	printf("versioning is in x.x.x format\n");
	printf("-h : display this message\n");
}
bool Data_Need_Swapping(void)
{
	int test_var = 1;
	const char* cptr = (char*)&test_var;

	//true means little ending
	//false means big endian
	return (cptr != NULL);
}
inline void endian_swap(unsigned int& x)
{
	x = (x >> 24) |
		((x << 8) & 0x00FF0000) |
		((x >> 8) & 0x0000FF00) |
		(x << 24);
}

char WriteV1UpdateFile(std::unique_ptr<UpdateStructV1>& update)
{
	//write the version file
	printf("writing v1 version file...\n");
	FILE* outputBin = fopen("version.bin", "wb");
	FILE* outputDat = fopen("version.dat", "wb");
	if (!outputBin || !outputDat)
	{
		if (outputBin)
			fclose(outputBin);
		if (outputDat)
			fclose(outputDat);

		printf("failed to open/create file!\n");
		return -1;
	}

	if (Data_Need_Swapping())
	{
		printf("the machine is %s endian. a endian swap is needed before writing\n", Data_Need_Swapping() ? "Little" : "Big");
		//swap hash to big endian 
		endian_swap(update->rc_version);
		endian_swap(update->rc_number);
		endian_swap(update->prod_version);
	}

	fwrite(update.get(), 1, sizeof(UpdateStructV1), outputBin);
	fwrite(update.get(), 1, sizeof(UpdateStructV1), outputDat);
	fclose(outputBin);
	fclose(outputDat);
	printf("done!\n");
	return 1;
}

char WriteUpdateFile(const std::unique_ptr<UpdateStruct>& update)
{
	//write the version file
	printf("writing v2 version file...\n");
	FILE* outputBin = fopen("versionV2.bin", "wb");
	FILE* outputDat = fopen("versionV2.dat", "wb");
	if (!outputBin || !outputDat)
	{
		if (outputBin)
			fclose(outputBin);
		if (outputDat)
			fclose(outputDat);

		printf("failed to open/create file!\n");
		return -1;
	}

	fwrite(update, 1, sizeof(UpdateStruct), outputBin);
	fwrite(update, 1, sizeof(UpdateStruct), outputDat);
	fclose(outputBin);
	fclose(outputDat);
	printf("done!\n");
	return 1;
}

char CalculateBinaryHash(const char* filename, unsigned int* hash)
{
	if (filename == NULL || hash == NULL)
		return -1;

	printf("opening dol...\n");
	FILE* dolFile = fopen(filename, "rb");
	if (!dolFile)
	{
		printf("failed to open %s\n", filename);
		return -2;
	}

	printf("opened\nreading Dol data...\n");

	// obtain file size:
	fseek(dolFile, 0, SEEK_END);
	long dolSize = ftell(dolFile);
	rewind(dolFile);

	SHA1 sha; // SHA-1 class
	sha.Reset();
	memset(hash, 0, 20);
	char input = fgetc(dolFile);
	for (int i = 0; i < dolSize; i++)
	{
		sha.Input(input);
		input = fgetc(dolFile);
	}
	fclose(dolFile);
	if (!sha.Result(hash))
	{
		printf("sha: could not compute Hash for stable release!\n");
		return -3;
	}

	printf("Hash : %08X %08X %08X %08X %08X\n",
		hash[0],
		hash[1],
		hash[2],
		hash[3],
		hash[4]);

	return 1;
}

int main(int argc, char **argv)
{
	printf("\t\tDacoTaco's Online Update Hash Generator\n");
	printf("\t\t-------------------------------------------\n\n");
	if (argc == 1 || (argc > 1 && (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"-H") == 0)))
	{
		Display_Parameters();
		exit(0);
	}
	else if(argc < 6)
	{
		printf("\n\nnot enough parameters given\n");
		Display_Parameters();
		exit(0);
	}

	/*for (int i = 1; i < argc; i++)
	{
		printf("%s\n",argv[i]);
	}
	sleep(2);*/

	char* InputProdFile = argv[1];
	char* InputRCFile = argv[3];
	unsigned int prodSHA1Hash[5];
	unsigned int rcSHA1Hash[5];
	int major;
	int minor;
	int patch;
	version_t version;
	version_t rc_version;
	memset(&version, 0, sizeof(version_t));
	memset(&rc_version, 0, sizeof(version_t));
	if (sscanf(argv[2], "%d.%d.%d", &major, &minor, &patch) != 3 || major > 254 || minor > 254 || patch > 254)
	{
		printf("Invalid prod version");
		goto _exit;
	}
	version.major = major & 0xFF;
	version.minor = minor & 0xFF;
	version.patch = patch & 0xFF;

	if (sscanf(argv[4], "%d.%d.%d", &major, &minor, &patch) != 3 || major > 254 || minor > 254 || patch > 254)
	{
		printf("Invalid prod version");
		goto _exit;
	}
	rc_version.major = major & 0xFF;
	rc_version.minor = minor & 0xFF;
	rc_version.patch = patch & 0xFF;
	rc_version.sub_version = atoi(argv[5]);

	printf("calculating Hash of -STABLE- dol version %u.%u.%u...\n", version.major, version.minor, version.patch);
	if( CalculateBinaryHash(InputProdFile, prodSHA1Hash) < 0)
		goto _exit;

	printf("calculating Hash of -RC- dol version %u.%u.%u RC %u...\n", rc_version.major, rc_version.minor, rc_version.patch, rc_version.sub_version);
	if( CalculateBinaryHash(InputRCFile, rcSHA1Hash) < 0)
		goto _exit;

	if (Data_Need_Swapping())
	{
		printf("the machine is %s endian. a endian swap is needed before writing\n\n", Data_Need_Swapping() ? "Little" : "Big");
		//swap hash to big endian 
		for (int i = 0; i < 5; i++)
		{
			endian_swap(prodSHA1Hash[i]);
			endian_swap(rcSHA1Hash[i]);
		}
	}

	//write the v2 version file
	auto UpdateFile = std::unique_ptr<UpdateStruct>(new UpdateStruct());
	if (!UpdateFile)
	{
		printf("failed to allocate UpdateFile\n");
		goto _exit;
	}
	memset(UpdateFile.get(), 0, sizeof(UpdateStruct));
	memcpy(UpdateFile->prod_sha1_hash, prodSHA1Hash, sizeof(UpdateFile->prod_sha1_hash));
	memcpy(UpdateFile->rc_sha1_hash, rcSHA1Hash, sizeof(UpdateFile->rc_sha1_hash));
	UpdateFile->prod_version = version;
	UpdateFile->rc_version = rc_version;
	if (WriteUpdateFile(UpdateFile) < 0)
	{
		goto _exit;
	}

	//write v1 version file
	auto UpdateFileV1 = std::unique_ptr<UpdateStructV1>(new UpdateStructV1());
	if (!UpdateFileV1)
	{
		printf("failed to allocate UpdateFile\n");
		goto _exit;
	}
	memset(UpdateFileV1.get(), 0, sizeof(UpdateStructV1));
	memcpy(UpdateFileV1->prod_sha1_hash, prodSHA1Hash, sizeof(UpdateFileV1->prod_sha1_hash));
	memcpy(UpdateFileV1->rc_sha1_hash, rcSHA1Hash, sizeof(UpdateFileV1->rc_sha1_hash));
	UpdateFileV1->prod_version = (unsigned int)((version.major << 8) | (version.minor*10) | (version.patch));
	UpdateFileV1->rc_version = (unsigned int)((rc_version.major << 8) | (rc_version.minor * 10) | (rc_version.patch));
	UpdateFileV1->rc_number = rc_version.sub_version;
	if (WriteV1UpdateFile(UpdateFileV1) < 0)
	{
		goto _exit;
	}

_exit:
	sleep(5);
	return 0;
}

