// XNFSMusicPlayerInstaller - the ingame installer!
// by Xanvier / xan1242

#include "..\includes\IniReader.h"
#include "..\includes\injector\hooking.hpp"
#include <direct.h>
#include <stdlib.h>

#define CONTINUELANGHASH 0x8098A54C
#define OKLANGHASH 0x417B2601
#define EXITLANGHASH 0xA9950B93

#define TYPEINFO 0
#define TYPEATTENTION 1
#define TYPESELECTION 3

#define YES1 0x59455331
#define YES2 0x59455332
#define YES3 0x59455333
#define YES4 0x59455334
#define YES5 0x59455335
#define YES6 0x59455336
#define YES7 0x59455337
#define YES8 0x59455338
#define YES9 0x59455339

#define NO01 0x4E4F3031
#define NO02 0x4E4F3032
#define NO03 0x4E4F3033
#define NO04 0x4E4F3034
#define NO05 0x4E4F3035
#define NO06 0x4E4F3036
#define NO07 0x4E4F3037
#define NO08 0x4E4F3038
#define NO09 0x4E4F3039

#define OK01 0x4F4B3031 
#define OK02 0x4F4B3032
#define OK03 0x4F4B3033
#define OK04 0x4F4B3034
#define OK05 0x4F4B3035
#define OK06 0x4F4B3036
#define OK07 0x4F4B3037
#define OK08 0x4F4B3038
#define OK09 0x4F4B3039

#define SELECTEDNO 1
#define SELECTEDYES 0

FILE *fin, *fin2, *batchfileout;
struct stat st = { 0 };

unsigned int DialogBoxFunction = 0x00598970;
unsigned int DialogBoxFunction2 = 0x598BB0;
unsigned int _free = 0x007C7250;
bool bInstallerCompleted = 0;
bool bInstallerSuccess = 0;
int ShowFileErrorDialog = 0;
bool bASFScannerFinished = 0;
bool bNodeScannerFinished = 0;
bool bScanForAllFiles = 0;
bool bInteractiveMusicFolderExists = 0;
bool bEnableInteractiveNoding = 1;
bool bDialogShownAtLeastOnce = 0;
bool bBreakThreadLoop = 0;
bool bUseOGGenc = 1;
bool bUseASF = 1;
float OGGEncQuality = 6.0;
int ButtonResult = 0;
int SystemReturnValue = 0;
unsigned int TheGameFlowManager = 0x00925E90;

const char IntroMessage[] = "Welcome to the XNFSMusicPlayer installer!\
^This will guide you through the basic first time interactive music setup procedure.\
^Before going any further, make sure you have enough disk space!^It is also recommended to run the game in a window as this might take a while.";

const char SXBatchScript[] = "for %%f in (\"InteractiveMusic/*.asf\") do scripts\\XNFSInstallerTools\\sx.exe -wave \"InteractiveMusic/%%f\" -=\"InteractiveMusic/%%f.wav\"\n\
@exit /b 69";

const char OGGEncBatchScript[] = "for %%%%f in (\"InteractiveMusic/*.wav\") do scripts\\XNFSInstallerTools\\oggenc2.exe -q %f \"InteractiveMusic/%%%%f\"\n\
@exit /b 68";

const char ASFCleanupScript[] = "del InteractiveMusic\\*.asf /Q\n\
@exit /b 67";

const char WAVCleanupScript[] = "del InteractiveMusic\\*.wav /Q\n\
@exit /b 66";

char* FilenameFormat;

char *BatchScriptBuffer;
char OGGEncMessageBuffer[127];

int ShowDialogBox(const char* message, int DialogType, unsigned int ButtonNameHash, unsigned int ButtonHash)
{
	_asm
	{
		push[message]
		push ButtonHash
		push ButtonHash
		push ButtonNameHash
		push DialogType
		push 0x00890978
		push 0x00890978
		call DialogBoxFunction
	}
	return 1;
}

int ShowDialogBox2(const char* message, int DialogType, unsigned int YesButtonHash, unsigned int NoButtonHash, int ButtonSelected)
{
	_asm
	{
		push[message]
		push ButtonSelected
		push NoButtonHash
		push NoButtonHash
		push YesButtonHash
		push 0x417B25E4
		push 0x70E01038
		push DialogType
		push 0x00890978
		push 0x00890978
		call DialogBoxFunction2
	}
	return 1;
}

void __declspec(naked) SnoopDatReturnValue(void* Pointer)
{
	_asm
	{
		mov eax, [esi + 0x2C]
		mov ButtonResult, eax
		jmp _free
	}
}

unsigned int ReadIDFromBank(FILE *fp)
{
	long int OldOffset = 0;
	unsigned int ID = 0;
	OldOffset = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(&ID, 4, 1, fp);
	fseek(fp, OldOffset, SEEK_SET);
	return ID;
}

long int SearchForBankIDOffset(FILE *fp, unsigned int IDtoSearch)
{
	long int OldOffset = 0;
	unsigned int CurrentID = 0;
	unsigned int IDOffset = 0;
	OldOffset = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	printf("Searching for ID: %x\n", IDtoSearch);
	while (!feof(fp))
	{
		fread(&CurrentID, 4, 1, fp);
		if (CurrentID == IDtoSearch)
		{
			fseek(fp, -4, SEEK_CUR);
			IDOffset = ftell(fp);
			printf("Found at %x\n", IDOffset);
			break;
		}
	}
	if (IDOffset == 0)
		printf("ERROR: ID %x not found.\n", IDtoSearch);
	fseek(fp, OldOffset, SEEK_SET);
	return IDOffset;
}

int OutputNodeInfoToFile(FILE *fp, unsigned int OffsetInMPF, const char* OutTxtFileName, const char* BaseFileNameFormat)
{
	FILE *fout = fopen(OutTxtFileName, "w");
	if (fout == NULL)
	{
		perror("ERROR");
		return (-1);
	}
	int i = 0;
	unsigned int NodeID = 0;
	unsigned int Time = 0;
	fseek(fp, OffsetInMPF + 0xC, SEEK_SET);
	while (!feof(fp))
	{
		if (fread(&NodeID, 4, 1, fp))
		{
			if ((i >= 0 && i <= 1313) || i >= 3240 && i <= 3256)
			{
				fprintf(fout, "%x = ", NodeID);
				fprintf(fout, BaseFileNameFormat, i);
				fprintf(fout, "\n");
			}
		}
		else
			break;
		if (fread(&Time, 4, 1, fp)) // leftover from other project
		{
			//printf("time ...\n");
			//fprintf(fout, "Time = %.3fs\t%d samples\n", (float)Time / 1000, (int)(((float)Time / 1000)*SampleRate));
		}
		else
			break;
		i++;
	}
	if (!feof(fp))
	{
		fclose(fout);
		return 0;
	}
	printf("Total number of nodes: %d\n", i);
	fclose(fout);
	return 1;
}

int ScanAndWriteASFs(FILE *fp, char* MainFileName)
{
	FILE *fout;
	void *FileBuffer;
	char OutFileName[267];
	unsigned int Magic = 0;
	unsigned int NextMagic = 0;
	unsigned int counter = 0;
	long unsigned int MagicOffset = 0;
	long unsigned int NextMagicOffset = 0;
	long unsigned int size = 0;
	long unsigned int totalsize = 0;
	fseek(fp, 0, SEEK_END);
	totalsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	printf("Searching for streams...\n");
	while (!feof(fp))
	{
		fread(&Magic, 4, 1, fp);
		if (Magic == 0x6C484353)
		{
			MagicOffset = ftell(fp) - 4;
			printf("Stream %d found at %X\t", counter, MagicOffset);
			while (!feof(fp))
			{
				fread(&NextMagic, 4, 1, fp);
				if (NextMagic == 0x6C484353)
				{
					NextMagicOffset = ftell(fp) - 4;;
					fseek(fp, MagicOffset, SEEK_SET);
					break;
				}
				NextMagicOffset = 0;
			}

			if (NextMagicOffset)
				size = NextMagicOffset - MagicOffset;
			else
			{
				fseek(fp, MagicOffset, SEEK_SET);
				size = totalsize - MagicOffset;
			}
			printf("Size: %X\n", size);

			sprintf(OutFileName, MainFileName, counter);
			if (bScanForAllFiles)
			{
				fout = fopen(OutFileName, "wb");
				FileBuffer = malloc(size);
				fread(FileBuffer, size, 1, fp);
				fwrite(FileBuffer, size, 1, fout);
				free(FileBuffer);
				fclose(fout);
			}
			else if ((counter >= 0 && counter <= 1313) || counter >= 3240)
			{
				fout = fopen(OutFileName, "wb");
				FileBuffer = malloc(size);
				fread(FileBuffer, size, 1, fp);
				fwrite(FileBuffer, size, 1, fout);
				free(FileBuffer);
				fclose(fout);
			}
			else
				printf("Skipping %d\n", counter);
			if (NextMagicOffset)
				fseek(fp, NextMagicOffset, SEEK_SET);
			else
				break;
			counter++;
		}
	}
	printf("Extraction successful!\n");
	bASFScannerFinished = 1;
	return 0;
}


DWORD WINAPI InstallerStateManager(LPVOID)
{
	while (1)
	{
		Sleep(10);
		if (bBreakThreadLoop)
			break;
		if (*(int*)TheGameFlowManager == 3 && !bDialogShownAtLeastOnce)
		{
			ShowDialogBox(IntroMessage, TYPEINFO, CONTINUELANGHASH, OK01);
			bDialogShownAtLeastOnce = 1;
		}
		if (SystemReturnValue == 69)
		{
			ShowDialogBox2("Decoding complete!^Do you want to clean up the ASF files?", TYPESELECTION, YES3, NO03, SELECTEDYES);
			SystemReturnValue = 0;
		}
		if (SystemReturnValue == 68)
		{
			ShowDialogBox2("OGG Encoding complete!^Do you want to clean up the WAV files?", TYPESELECTION, YES4, NO04, SELECTEDYES);
			SystemReturnValue = 0;
		}
		if (SystemReturnValue == 67)
		{
			if(bUseOGGenc)
				ShowDialogBox("ASF cleanup complete!^The installer will now start the encoding process.", TYPEATTENTION, CONTINUELANGHASH, OK06);
			else
				ShowDialogBox("ASF cleanup complete!^The installer will now generate a node path definition file.", TYPEATTENTION, CONTINUELANGHASH, OK07);
			SystemReturnValue = 0;
		}
		if (SystemReturnValue == 66)
		{
			ShowDialogBox("WAV cleanup complete!^The installer will now generate a node path definition file.", TYPEATTENTION, CONTINUELANGHASH, OK07);
			SystemReturnValue = 0;
		}
		if (bNodeScannerFinished)
		{
			ShowDialogBox("Installation successful! Enjoy XNFSMusicPlayer!^As this installer is very limited, make sure to further tune your settings in XNFSMusicPlayer.ini which is found in the scripts folder!^The game will now quit.", TYPEINFO, EXITLANGHASH, OK02);
			bInstallerSuccess = 1;
			bNodeScannerFinished = 0;
		}
		if (ShowFileErrorDialog == 1)
		{
			ShowDialogBox("Error opening Sound\\PFData\\MW_Music.mus^Check if the file exists and/or if it is accessible by permissions.^The installer will now quit.", TYPEATTENTION, OKLANGHASH, OK02);
			ShowFileErrorDialog = 0;
		}
		if (ShowFileErrorDialog == 2)
		{
			ShowDialogBox("Error opening Sound\\PFData\\MW_Music.mpf^Check if the file exists and/or if it is accessible by permissions.^The installer will now quit.", TYPEATTENTION, OKLANGHASH, OK02);
			ShowFileErrorDialog = 0;
		}
		if (bASFScannerFinished)
		{
			if (bUseASF)
				ShowDialogBox("Extraction of MW_Music.mus sucessful!^Will start to generate a definition file after confirmation.", TYPEATTENTION, CONTINUELANGHASH, OK07);
			else
				ShowDialogBox("Extraction of MW_Music.mus sucessful!^Will start to decode files after confirmation.", TYPEATTENTION, CONTINUELANGHASH, OK05);
			bASFScannerFinished = 0;
		}

		switch (ButtonResult)
		{
			if (bDialogShownAtLeastOnce && *(int*)TheGameFlowManager == 3)
			{
		case OK01:
			ShowDialogBox2("Do you want to use interactive music?", TYPESELECTION, YES1, NO01, SELECTEDYES);
			break;
		case NO01:
			ShowDialogBox("OK! Not using interactive music!^As this installer is very limited, make sure to further tune your settings in XNFSMusicPlayer.ini which is found in the scripts folder!^The game will now quit.", TYPEINFO, EXITLANGHASH, OK02);
			bInstallerSuccess = 1;
			bEnableInteractiveNoding = 0;
			break;
		case OK02:
			bBreakThreadLoop = 1;
			break;
		case YES1:
			ShowDialogBox("This installer will now extract the necessary data from the game in order to bring compatibility for the BASS library.^By default it will be in the InteractiveMusic directory.^You can change this later in XNFSMusicPlayer_NodePaths.txt which is found in the scripts folder.", TYPEINFO, CONTINUELANGHASH, OK03);
			break;
		case OK03:
			ShowDialogBox2("Do you want to use ASF files directly without conversion?^Please note that this is the recommended way as there is no data loss from the original and is the quickest way of installing.", TYPESELECTION, YES2, NO05, SELECTEDYES);
			break;
		case NO05:
			bUseASF = 0;
			ShowDialogBox2("Do you want to use OGG encoding to save disk space?^WARNING: This will increase time required to install.", TYPEATTENTION, YES2, NO02, SELECTEDYES);
			break;
		case NO02:
			bUseOGGenc = 0;
			ShowDialogBox("OK! Not using OGG encoding!^This installer will now proceed to extract and decode streams from Sound\\PFData\\MW_Music.mus...", TYPEINFO, CONTINUELANGHASH, OK04);
			break;
		case YES2:
			ShowDialogBox("This installer will now proceed to extract streams from Sound\\PFData\\MW_Music.mus.", TYPEATTENTION, CONTINUELANGHASH, OK04);
			break;
		case OK04:
			if (stat("InteractiveMusic", &st) == -1)
				mkdir("InteractiveMusic");
			else
				bInteractiveMusicFolderExists = 1;
			if (bInteractiveMusicFolderExists)
			{
				Sleep(10);
				ShowDialogBox("Folder InteractiveMusic already exists. Assuming all files are extracted already.^If you need to extract again, delete/rename the folder and try again.", TYPEATTENTION, CONTINUELANGHASH, OK05);
			}
			else
			{
			ShowDialogBox("Please wait until the ASF scanner has finished extracting...^Dialog will automatically update when it's finished.^You can check the console for more info of what's happening.", TYPEATTENTION, 0x0FFFFFFF, 0x0FFFFFFF);
			fin = fopen("SOUND\\PFDATA\\MW_Music.mus", "rb");
			if (fin == NULL)
				ShowFileErrorDialog = 1;
			ScanAndWriteASFs(fin, "InteractiveMusic\\MW_Music.mus_%d.asf");
			fclose(fin);
			}
			break;
		case OK05:
			if (bInteractiveMusicFolderExists)
			{
				ShowDialogBox("Skipping the decoding since InteractiveMusic already exists.^The installer will now generate a node path definition file.", TYPEATTENTION, OKLANGHASH, OK07);
			}
			else
			{
				ShowDialogBox("Decoding ASFs to WAV with Sound eXchange...", TYPEATTENTION, 0x0FFFFFFF, 0x0FFFFFFF);
				batchfileout = fopen("XNFSTempBatch.bat", "w");
				fwrite(SXBatchScript, strlen(SXBatchScript), 1, batchfileout);
				fclose(batchfileout);
				SystemReturnValue = system("XNFSTempBatch.bat");
				remove("XNFSTempBatch.bat");
			}
			break;
		case YES3:
			ShowDialogBox("Cleaning up ASFs...", TYPEATTENTION, 0x0FFFFFFF, 0x0FFFFFFF);
			batchfileout = fopen("XNFSTempBatch3.bat", "w");
			fwrite(ASFCleanupScript, strlen(ASFCleanupScript), 1, batchfileout);
			fclose(batchfileout);
			SystemReturnValue = system("XNFSTempBatch3.bat");
			remove("XNFSTempBatch3.bat");
			break;
		case NO03:
			if (bUseOGGenc)
				ShowDialogBox("OK! Not cleaning up ASFs!^If you have opted to encode to OGG, the process will begin now.", TYPEATTENTION, CONTINUELANGHASH, OK06);
			else
				ShowDialogBox("OK! Not cleaning up ASFs!^The installer will now generate a node path definition file.", TYPEATTENTION, CONTINUELANGHASH, OK07);
			break;
		case YES4:
			ShowDialogBox("Cleaning up WAVs...", TYPEATTENTION, 0x0FFFFFFF, 0x0FFFFFFF);
			batchfileout = fopen("XNFSTempBatch4.bat", "w");
			fwrite(WAVCleanupScript, strlen(WAVCleanupScript), 1, batchfileout);
			fclose(batchfileout);
			SystemReturnValue = system("XNFSTempBatch4.bat");
			remove("XNFSTempBatch4.bat");
			break;
		case NO04:
			ShowDialogBox("OK! Not cleaning up WAVs!^The installer will now generate a node path definition file.", TYPEATTENTION, CONTINUELANGHASH, OK07);
			break;
		case OK06:
			if (bUseOGGenc)
			{
				sprintf(OGGEncMessageBuffer, "Encoding WAVs to OGG with %f quality...", OGGEncQuality);
				ShowDialogBox(OGGEncMessageBuffer, TYPEATTENTION, 0x0FFFFFFF, 0x0FFFFFFF);
				BatchScriptBuffer = (char*)malloc(strlen(OGGEncBatchScript));
				sprintf(BatchScriptBuffer, OGGEncBatchScript, OGGEncQuality);
				batchfileout = fopen("XNFSTempBatch2.bat", "w");
				fwrite(BatchScriptBuffer, strlen(BatchScriptBuffer), 1, batchfileout);
				fclose(batchfileout);
				SystemReturnValue = system("XNFSTempBatch2.bat");
				remove("XNFSTempBatch2.bat");
			}
			break;
		case OK07:
			ShowDialogBox("Extracting node information from MW_Music.mpf...", TYPEATTENTION, 0x0FFFFFFF, 0x0FFFFFFF);
			fin = fopen("SOUND\\PFDATA\\MW_Music.mus", "rb");
			if (fin == NULL)
			{
				ShowFileErrorDialog = 1;
				break;
			}
			fin2 = fopen("SOUND\\PFDATA\\MW_Music.mpf", "rb");
			if (fin2 == NULL)
			{
				ShowFileErrorDialog = 2;
				break;
			}
			if (bUseASF)
				FilenameFormat = "InteractiveMusic\\MW_Music.mus_%d.asf";
			else if (bUseOGGenc)
				FilenameFormat = "InteractiveMusic\\MW_Music.mus_%d.asf.ogg";
			else
				FilenameFormat = "InteractiveMusic\\MW_Music.mus_%d.asf.wav";
			bNodeScannerFinished = OutputNodeInfoToFile(fin2, SearchForBankIDOffset(fin2, ReadIDFromBank(fin)), "scripts\\XNFSMusicPlayer_NodePaths.txt", FilenameFormat);
			fclose(fin);
			fclose(fin2);
			break;
			}
		}
		ButtonResult = 0;
	}
	if (bInstallerSuccess)
	{
		CIniReader inireader("XNFSMusicPlayer.ini");
		if (!bEnableInteractiveNoding)
			inireader.WriteInteger("XNFSMusicPlayer", "EnableInteractiveNoding", 0);
		else
			inireader.WriteInteger("XNFSMusicPlayer", "EnableInteractiveNoding", 1);
		inireader.WriteInteger("Installer", "InstallerCompleted", 1);
	}
	injector::WriteMemory<int>(0x9257EC, 0x1, true);
	return 0;
}

int init()
{
	CIniReader inireader("XNFSMusicPlayer.ini");
	bInstallerCompleted = inireader.ReadInteger("Installer", "InstallerCompleted", 0);
	bScanForAllFiles = inireader.ReadInteger("Installer", "ScanForAllFiles", 0);
	OGGEncQuality = inireader.ReadFloat("Installer", "OGGEncQuality", 6.0);
	return 0;
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		freopen("CON", "w", stdout);
		freopen("CON", "w", stderr);
		init();
		if (!bInstallerCompleted)
		{
			injector::WriteMemory<int>(0x008F3C58, 0x00894920, true);
			injector::MakeCALL(0x0058F710, SnoopDatReturnValue, true);
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&InstallerStateManager, NULL, 0, NULL);
		}
	}
	return TRUE;
}
