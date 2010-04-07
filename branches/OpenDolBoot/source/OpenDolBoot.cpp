// OpenDolBoot.cpp : Defines the entry point for the console application.
//

#include "../include/OpenDolBoot.h"


int main(int argc, char **argv)
{
	printf("DacoTaco and _Dax_ 's OpenDolboot : C/C++ Version %s\n\n",VERSION);
	char* InputFile = 0;
	char ShowInfo = false;
	char* Nand_Use_File = 0;
	char* OutputFile = 0;
	if ( argc < 1 && (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"-H") == 0 || argc == 1))
	{
		Display_Parameters();
		exit(0);
	}
	else if(argc < 3)
	{
		Display_Parameters();
		printf("\n\nnot enough parameters given\n");
		exit(0);
	}
	else if( strncmp(argv[1],"-x",1) == 0 )
	{
		Display_Parameters();
		printf("\n\nno input dol given\n");
		exit(0);
	}
	for (int i = 2; i < argc; i++)
	{
#ifdef DEBUG
		printf("parameter %d of %d : %s\n",i,argc,argv[i]);
#endif
		if (strcmp(argv[i],"-h") == 0 || strcmp(argv[i],"-H") == 0 || argc == 1)
		{
			Display_Parameters();
			exit(0);
		}
		if (strcmp(argv[i],"-i") == 0 || strcmp(argv[i],"-I") == 0)
		{
			ShowInfo = true;
		}
		else if (strcmp(argv[i],"-n") == 0 || strcmp(argv[i],"-N") == 0)
		{
			if(i+1 >= argc)
			{
				printf("no filename for nand code given\n");
				exit(0);
			}
			else if(strncmp(argv[i+1],"-x",1) == 0 )
			{
				printf("parameter given instead of filename for nand code!\n");
				exit(0);
			}
			else
			{
				i++;
				Nand_Use_File = argv[i];
#ifdef DEBUG
				printf("using nand file %s\n",Nand_Use_File);
#endif
			}
		}
	}
	if (Nand_Use_File != NULL)
	{
		if(ShowInfo == false && (  strncmp(argv[argc-1],"-x",1) == 0 || strcmp(argv[argc-1],Nand_Use_File) == 0 || strcmp(argv[argc-1],argv[1]) == 0 ) )
		{
			Display_Parameters();
			printf("\n\nno output app given\n");
			exit(0);
		}
	}
	else if( ShowInfo == false && (  strncmp(argv[argc-1],"-x",1) == 0  || strcmp(argv[argc-1],argv[1]) == 0 ) )
	{
		Display_Parameters();
		printf("\n\nno output app given\n");
		exit(0);
	}
	if(!ShowInfo)
	{
		OutputFile = argv[argc-1];
	}
	InputFile = argv[1];
	dolhdr DolHeader;
	FILE* Dol;
	printf("opening dol...\n");
	Dol = fopen(InputFile,"rb");
	if(!Dol)
	{
		printf("failed to open %s\n",InputFile);
		exit(0);
	}
	printf("opened\nreading data Dol info (header and data)...\n");
	long DolSize;
	char* DolFile;
	// obtain file size:
	fseek (Dol , 0 , SEEK_END);
	DolSize = ftell (Dol) - sizeof(dolhdr);
#ifdef DEBUG
	printf("detected file size : %d-0x%x\n",(int)DolSize,(int)DolSize);
#endif
	rewind (Dol);
	size_t result = fread(&DolHeader,1,sizeof(dolhdr),Dol);
	if (result != sizeof(dolhdr)) 
	{
		printf("error reading dol header.Reading error: %d bytes of %d read\n",result,sizeof(dolhdr)); 
		exit(0);
	}
	// allocate memory to contain the whole file:
	DolFile = (char*) malloc (DolSize);//Allocate memory to contain the whole file + the nand shit
	if (DolFile == NULL) 
	{
		printf("Memory error");
		exit(0);
	}
	memset(DolFile,0,DolSize);
	//copy the file into the buffer:
	result = fread(DolFile,1,DolSize,Dol);
	if (result != DolSize) 
	{
		printf("error reading Dol Data.Reading error: %d bytes of %d read\n",result,(int)DolSize); 
		exit(0);
	}
#ifdef DEBUG
	printf("done reading %d bytes into memory\n",(int)DolSize);
#else
	printf("done reading data\n");
#endif

	if (ShowInfo)
	{
		printf("\n\n");
		printf("entrypoint: %08X\tBSS Address : %08X\tBSS Size: %08X\n\n", (unsigned int)SwapEndian(DolHeader.entrypoint) , (unsigned int)SwapEndian(DolHeader.addressBSS) , (unsigned int)SwapEndian(DolHeader.sizeBSS) );
		printf("Displaying Text info:\n");
		for(int i = 0;i < 6;i++)
		{
			if(DolHeader.sizeText[i] && DolHeader.addressText[i] && DolHeader.offsetText[i])
			{
				//valid info. swap and display
				printf("offset : %08x\taddress : %08x\tsize : %08x\t\t\n", (unsigned int)SwapEndian(DolHeader.offsetText[i]), (unsigned int)SwapEndian(DolHeader.addressText[i]), (unsigned int)SwapEndian(DolHeader.sizeText[i]));
			}
		}
		printf("\nDisplaying Data Info:\n");
		for(int i = 0;i <= 10;i++)
		{
			if(DolHeader.sizeData[i] && DolHeader.addressData[i] && DolHeader.offsetData[i])
			{
				//valid info. swap and display
				printf("offset : %08x\taddress : %08x\tsize : %08x\t\t\n", (unsigned int)SwapEndian(DolHeader.offsetData[i]), (unsigned int)SwapEndian(DolHeader.addressData[i]), (unsigned int)SwapEndian(DolHeader.sizeData[i]));
			}
		}
		sleep(5);
		exit(0);
	}
	printf("starting nand loader injection...\n");
	FILE* nboot_fd = NULL;
	Nandcode nboot;
	long nbootSize;
	if (Nand_Use_File != NULL)
	{
#ifdef DEBUG
		printf("opening & putting the nboot in memory...");
#endif
		nboot_fd = fopen(Nand_Use_File,"rb");
		if(!nboot_fd)
		{
		  printf("failed to open %s\n",Nand_Use_File);
		  exit(0);
		}
		fseek (nboot_fd , 0 , SEEK_END);
		nbootSize = ftell (nboot_fd);
		rewind (nboot_fd);
		if( nbootSize != 1296)
		{
		  printf("\nnboot.bin isn't the correct size!\n");
		  exit(0);
		}
		result = fread(&nboot,1,nbootSize,nboot_fd);
		if (result != nbootSize)
		{
		  printf("\nreading error: %d & %d",result,(int)nbootSize);
		  exit(0);
		}
		fclose(nboot_fd);
#ifdef DEBUG
		printf("Done\n");
#endif
	}
	else
	{
		memcpy((void*)&nboot,_nboot, _nboot_size);
		nbootSize = _nboot_size;
	}

	printf("checking & editing data before saving...");
	if(DolHeader.sizeText[1] && DolHeader.addressText[1] && DolHeader.offsetText[1])
	{
		//o ow, text2 is already full. lets quit before we brick ppl
		printf("\n\nText2 already contains data! quiting out of failsafe...\n");
		exit(0);
	}
	if (nboot.entrypoint_dol != DolHeader.entrypoint)
	{
		printf("\n\ndifferent nboot to dol entrypoint detected! Changing\n\t0x%08X\tto\t0x%08X\n",(unsigned int)SwapEndian(nboot.entrypoint_dol),(unsigned int)SwapEndian(DolHeader.entrypoint));
		nboot.entrypoint_dol = DolHeader.entrypoint;

	}
	int nbootOffset = ftell (Dol);
	int padding = nbootOffset%16;
#ifdef DEBUG
	printf("dol is at offset 0x%08X\n",nbootOffset);
#endif
	if ( padding != NULL )
	{
		printf("\n\nneeded to add %d bytes of padding at the end of the dol\n",padding);
		//add the padding to the offset or its bogus
		DolHeader.offsetText[1] = SwapEndian(nbootOffset + 0x10 + padding);
		exit(0);
	}
	else
	{
		DolHeader.offsetText[1] = SwapEndian(nbootOffset + 0x10);
	}
	DolHeader.sizeText[1] = SwapEndian(nbootSize-0x10);
	DolHeader.addressText[1] = SwapEndian(0x80003400);

	printf("done!\n\nSaving to output.app...\n");
	//done! lets save this fucker! :D
	FILE* output = fopen(OutputFile,"wb+");
	if(!output)
	{
		printf("failed to create new file for writing!\n");
		exit(0);
	}
	fwrite(&DolHeader,1,sizeof(dolhdr),output);
	fwrite(DolFile,1,DolSize,output);
	if ( padding != NULL )
	{
		printf("adding padding...\n");
		fwrite(0,1,padding,Dol);
	}
	fwrite(&nboot,1,nbootSize,output);
	fclose(output);
	printf("DONE! check %s to verify data!\n",OutputFile);
	if(DolFile)
		free(DolFile);
	return 0;
}

void Display_Parameters ( void )
{
	printf("OpenDolBoot Input_Dol_filename [-i] [-n nandcode_filename] [-h] Output_App_filename \n\n");
	printf("parameters:\n");
	printf("-i : display info about the dol file and exit (no other parameters are required when using -i\n");
	printf("-n : use the following nand code and not the default nboot.bin\n");
	printf("-h : display this message\n");
}
