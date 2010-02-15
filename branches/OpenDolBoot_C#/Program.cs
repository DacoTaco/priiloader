using System;
using System.Net;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Reflection;

namespace OpenDolBoot
{
    class Program
    {
        struct DOL_Header
        {
            public uint[] TextOffsets;
            public uint[] DataOffsets;
            public uint[] TextLoadingAddrs;
            public uint[] DataLoadingAddrs;
            public uint[] TextSectionSizes;
            public uint[] DataSectionSizes;
            public uint BSS_Address;
            public uint BSS_Size;
            public uint Entrypoint;
            public byte[] padding;

            //initialize our header to a generic, all zeroes header
            public void Init()
            {
                 TextOffsets = new uint[7];
                 DataOffsets = new uint[11];
                 TextLoadingAddrs = new uint[7];
                 DataLoadingAddrs = new uint[11];
                 TextSectionSizes = new uint[7];
                 DataSectionSizes = new uint[11];
                 BSS_Address = 0x00000000;
                 BSS_Size = 0x00000000;
                 Entrypoint = 0x00000000;
                 padding = new byte[28];
            }

            //read in our header, doing an endian swap in case we print it onscreen
            public void ReadDOLHeader(BinaryReader br)
            {
                if (br == null)
                    return;

                for (int i = 0; i < TextOffsets.Length; i++)
                    TextOffsets[i] = SwapU32(br.ReadUInt32());

                for (int i = 0; i < DataOffsets.Length; i++)
                    DataOffsets[i] = SwapU32(br.ReadUInt32());

                for (int i = 0; i < TextLoadingAddrs.Length; i++)
                    TextLoadingAddrs[i] = SwapU32(br.ReadUInt32());

                for (int i = 0; i < DataLoadingAddrs.Length; i++)
                    DataLoadingAddrs[i] = SwapU32(br.ReadUInt32());

                for (int i = 0; i < TextSectionSizes.Length; i++)
                    TextSectionSizes[i] = SwapU32(br.ReadUInt32());

                for (int i = 0; i < DataSectionSizes.Length; i++)
                    DataSectionSizes[i] = SwapU32(br.ReadUInt32());

                BSS_Address = SwapU32(br.ReadUInt32());
                BSS_Size = SwapU32(br.ReadUInt32());
                Entrypoint = SwapU32(br.ReadUInt32());
            }
        }


        static void Main(string[] args)
        {
            DOL_Header header = new DOL_Header();
            header.Init();
            uint DynamicEntry = 0x80003400;

            //if (args.Length == 1 && args[0] != null)
            //{
            //    uint.TryParse(args[0], out DynamicEntry);
            //}
            FileStream fs = new FileStream("boot.dol", FileMode.Open);

            if (fs != null)
            {
                //Console.WriteLine("Opened {0}", args[0]);

                BinaryReader br = new BinaryReader(fs);
                if (br == null)
                {
                    Console.WriteLine("Couldn't open Binary stream. Exiting...");
                    return;
                }

                header.ReadDOLHeader(br);
                PrintDOLHeader(header);
                ConvertToSysMenuCompatible(header, br, DynamicEntry);
                fs.Close();
            }
        }

        static void ConvertToSysMenuCompatible(DOL_Header header, BinaryReader boot, uint DynamicEntry)
        {
            FileStream file = new FileStream("compatible.app", FileMode.Create);
            if (file == null)
            {
                Console.WriteLine("Couldn't create compatible.app. Returning.");
                return;
            }

            BinaryWriter bw = new BinaryWriter(file);
            if (bw == null)
                return;

            FileStream nandbootinfo = new FileStream("nboot.bin", FileMode.Open);
            if (nandbootinfo == null)
            {
                Console.WriteLine("Couldn't read nboot.bin. Returning.");
                return;
            }
            BinaryReader nreader = new BinaryReader(nandbootinfo);
            if (nreader == null)
                return;

            //check for the first blank entry and add our nand boot code in it
            for (int i = 0; i < 7; i++)
            {
                if (header.TextOffsets[i] == 0x0 && header.TextLoadingAddrs[i] == 0x0 && header.TextSectionSizes[i] == 0x0)
                {
                    header.TextOffsets[i] = (uint)((boot.BaseStream.Length));
                    header.TextLoadingAddrs[i] = DynamicEntry;
                    header.TextSectionSizes[i] = (uint)nreader.BaseStream.Length;
                    break;
                }
            }
            
            //now, write our entire header, and also swap it back, so it writes back to big endian properly
            for(int i = 0; i < 7; i++)
                bw.Write(SwapU32(header.TextOffsets[i]));

            for (int i = 0; i < 11; i++)
                bw.Write(SwapU32(header.DataOffsets[i]));

            for (int i = 0; i < 7; i++)
                bw.Write(SwapU32(header.TextLoadingAddrs[i]));

            for (int i = 0; i < 11; i++)
                bw.Write(SwapU32(header.DataLoadingAddrs[i]));

            for (int i = 0; i < 7; i++)
                bw.Write(SwapU32(header.TextSectionSizes[i]));

            for (int i = 0; i < 11; i++)
                bw.Write(SwapU32(header.DataSectionSizes[i]));

            bw.Write(SwapU32(header.BSS_Address));
            bw.Write(SwapU32(header.BSS_Size));
            bw.Write(SwapU32(header.Entrypoint));
            bw.Write(header.padding);

            boot.BaseStream.Position = 0x100; //skip the other header

            bw.Write(boot.ReadBytes((int)boot.BaseStream.Length)); //write the dol
            bw.Write(nreader.ReadBytes((int)nreader.BaseStream.Length), 0, 0x3FC); //write nand boot
            bw.Write(SwapU32(header.Entrypoint));
            nreader.BaseStream.Position = 0;
            bw.Write(nreader.ReadBytes((int)nreader.BaseStream.Length), 0x400, 0x100);
            bw.BaseStream.Close();
            nreader.BaseStream.Close();
        }

        static void PrintDOLHeader(DOL_Header header)
        {
            Console.WriteLine("Header contents: ");
            Console.WriteLine("BSS Address: 0x{0}", header.BSS_Address.ToString("X8"));
            Console.WriteLine("BSS Size: 0x{0}", header.BSS_Size.ToString("X8"));
            Console.WriteLine("Entry point: 0x{0}", header.Entrypoint.ToString("X8"));
            Console.WriteLine();

            //print the text/data sections, skip blank ones
            for(int i = 0; i < 7; i++)
                if(header.TextOffsets[i] != 0 && header.TextLoadingAddrs[i] != 0 && header.TextSectionSizes[i] != 0)
                    Console.WriteLine("Text section {3}: Offset = 0x{0} Address = 0x{1} Size = 0x{2}", header.TextOffsets[i].ToString("X8"), header.TextLoadingAddrs[i].ToString("X8"), header.TextSectionSizes[i].ToString("X8"), i);

            Console.WriteLine();
            for (int i = 0; i < 11; i++)
                if (header.DataOffsets[i] != 0 && header.DataLoadingAddrs[i] != 0 && header.DataSectionSizes[i] != 0)
                    Console.WriteLine("Data section {3}: Offset = 0x{0} Address = 0x{1} Size = 0x{2}", header.DataOffsets[i].ToString("X8"), header.DataLoadingAddrs[i].ToString("X8"), header.DataSectionSizes[i].ToString("X8"), i);
            Console.Read();
        }

        static uint SwapU32(uint num)
        {
            return (uint)IPAddress.HostToNetworkOrder((int)num);
        }
    }
}