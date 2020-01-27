// HashGenerator.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../../Shared/sha1.h"
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
	unsigned int version;
	unsigned SHA1_Hash[5];
	unsigned int beta_version;
	unsigned int beta_number;
	unsigned beta_SHA1_Hash[5];
} UpdateStruct;

void Display_Parameters ( void )
{
	printf("HashGenerator [stable_dol_path] stable_version [beta_dol_path] beta_version beta_number\n\n");
	printf("parameters:\n");
	printf("-h : display this message\n");
}
bool Data_Need_Swapping( void )
{
    int test_var = 1;
    char *cptr = (char*)&test_var;

	//true means little ending
	//false means big endian
    return (cptr != NULL);
}
inline void endian_swap(unsigned int& x)
{
    x = (x>>24) | 
        ((x<<8) & 0x00FF0000) |
        ((x>>8) & 0x0000FF00) |
        (x<<24);
}


int main(int argc, char **argv)
{
	printf("\t\tDacoTaco's Online Update Hash Generator\n");
	printf("\t\t-------------------------------------------\n\n");
	if ( argc < 1 && (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"-H") == 0 || argc == 1))
	{
		Display_Parameters();
		exit(0);
	}
	else if(argc < 6)
	{
		printf("\n\nnot enough parameters given\n");
		Display_Parameters();
		exit(0);
	}/*
	for (int i = 1; i < argc; i++)
	{
		printf("%s\n",argv[i]);
	}
	sleep(2);*/
	char* InputStableFile = argv[1];
	int version = atoi((const char*)argv[2]);
	char* InputBetaFile = argv[3];
	int beta_version = atoi((const char*)argv[4]);
	unsigned char beta_number = atoi((const char*)argv[5]);
	FILE* Dol;
	UpdateStruct *UpdateFile;
	UpdateFile = (UpdateStruct*) malloc(sizeof(UpdateStruct));
	if(UpdateFile == NULL)
	{
		printf("failed to allocate UpdateFile\n");
		sleep(2);
		exit(0);
	}
	memset(UpdateFile,0,sizeof(UpdateStruct));
	printf("opening dol...\n");
	Dol = fopen(InputStableFile,"rb");
	if(!Dol)
	{
		printf("failed to open %s\n",InputStableFile);
		sleep(2);
		free(UpdateFile);
		exit(0);
	}
	printf("opened\nreading Dol data...\n");
	long DolSize;
	// obtain file size:
	fseek (Dol , 0 , SEEK_END);
	DolSize = ftell(Dol);
#ifdef DEBUG
	printf("detected file size : %d-0x%x\n",(int)DolSize,(int)DolSize);
#endif
	rewind (Dol);
	char DolFile;
	UpdateFile->version = (int)version;
	printf("calculating Hash of -STABLE- dol version %u...\n",UpdateFile->version);
	SHA1 sha; // SHA-1 class
	sha.Reset();
    DolFile = fgetc(Dol);
	for(int i = 0; i < DolSize;i++)
	{
		sha.Input(DolFile);
        DolFile = fgetc(Dol);
	}
    fclose(Dol);
    if (!sha.Result(UpdateFile->SHA1_Hash))
    {
        printf("sha: could not compute Hash for stable release!\n");
		free(UpdateFile);
		sleep(2);
		exit(0);
    }
    else
    {
		printf( "Hash : %08X %08X %08X %08X %08X\n",
				UpdateFile->SHA1_Hash[0],
				UpdateFile->SHA1_Hash[1],
				UpdateFile->SHA1_Hash[2],
				UpdateFile->SHA1_Hash[3],
				UpdateFile->SHA1_Hash[4]);
    }

	//done with stable release. now to do the same for beta lol
	DolSize = 0;
	printf("opening dol...\n");
	Dol = fopen(InputBetaFile,"rb");
	if(!Dol)
	{
		printf("failed to open %s\n",InputBetaFile);
		sleep(2);
		free(UpdateFile);
		exit(0);
	}
	printf("opened\nreading Dol data...\n");
	fseek (Dol , 0 , SEEK_END);
	DolSize = ftell(Dol);
	rewind (Dol);
	UpdateFile->beta_version = (int)beta_version;
	UpdateFile->beta_number = (int)beta_number;
	printf("calculating Hash of -BETA- dol version %u beta %u...\n",UpdateFile->beta_version , UpdateFile->beta_number);
	sha.Reset();
    DolFile = fgetc(Dol);
	for(int i = 0; i < DolSize;i++)
	{
		sha.Input(DolFile);
        DolFile = fgetc(Dol);
	}
    fclose(Dol);
    if (!sha.Result(UpdateFile->beta_SHA1_Hash))
    {
        printf("sha: could not compute Hash for Official release!\n");
		free(UpdateFile);
		sleep(2);
		exit(0);
    }
    else
    {
		printf( "Hash : %08X %08X %08X %08X %08X\n",
				UpdateFile->beta_SHA1_Hash[0],
				UpdateFile->beta_SHA1_Hash[1],
				UpdateFile->beta_SHA1_Hash[2],
				UpdateFile->beta_SHA1_Hash[3],
				UpdateFile->beta_SHA1_Hash[4]);
    }


	//write the version file
	printf("writing version file...\n");
	FILE *output = fopen("version.dat","wb");
	if(!output)
	{
		printf("failed to open/create file!\n");
		sleep(2);
		free(UpdateFile);
		exit(0);
	}
	if( Data_Need_Swapping() )
	{
		printf("the machine is %s endian. a endian swap is needed before writing\n\n",Data_Need_Swapping()?"Little":"Big");
		//swap hash to big endian 
		for (int i = 0;i < 5;i++)
		{
			endian_swap(UpdateFile->SHA1_Hash[i]);
			endian_swap(UpdateFile->beta_SHA1_Hash[i]);
		}
		endian_swap(UpdateFile->beta_version);
		endian_swap(UpdateFile->beta_number);
		endian_swap(UpdateFile->version);
	}
	fwrite(UpdateFile,1,sizeof(UpdateStruct),output);
	fclose(output);
	printf("done!\n");
	sleep(5);
	free(UpdateFile);
	return 0;
}

