/*
 * Copyright © Oriel contributors
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"),to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

namespace Windower.Core
{
    using System;
    using System.IO;
    using System.Runtime.InteropServices;

    /// <summary>
    /// Reads and sets the IMAGE_FILE_LARGE_ADDRESS_AWARE flag on a 32-bit executable, which lets
    /// the process use up to 4 GB of address space on 64-bit Windows instead of 2 GB.
    /// </summary>
    internal static class LargeAddressAware
    {
        private const ushort ImageFileLargeAddressAware = 0x0020;
        private const ushort ImageFileMachineI386 = 0x014C;
        private const ushort OptionalHeaderMagicPE32 = 0x010B;
        private const uint PESignature = 0x00004550; // "PE\0\0"

        /// <summary>
        /// Returns whether the executable at <paramref name="path"/> has the large address aware flag.
        /// </summary>
        /// <exception cref="InvalidDataException">The file is not a 32-bit x86 executable.</exception>
        public static bool IsEnabled(string path)
        {
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            {
                var header = ReadHeader(stream);
                return (header.Characteristics & ImageFileLargeAddressAware) != 0;
            }
        }

        /// <summary>
        /// Sets the large address aware flag on the executable at <paramref name="path"/> if it is not
        /// already set, and updates the header checksum.
        /// </summary>
        /// <returns><c>true</c> if the file was changed; <c>false</c> if the flag was already set.</returns>
        /// <exception cref="InvalidDataException">The file is not a 32-bit x86 executable.</exception>
        /// <exception cref="IOException">The file is in use or could not be written.</exception>
        /// <exception cref="UnauthorizedAccessException">The file is not writable by this user.</exception>
        public static bool Enable(string path)
        {
            long checksumOffset;
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.Read))
            {
                var header = ReadHeader(stream);
                if ((header.Characteristics & ImageFileLargeAddressAware) != 0)
                {
                    return false;
                }

                var characteristics = (ushort)(header.Characteristics | ImageFileLargeAddressAware);
                stream.Seek(header.CharacteristicsOffset, SeekOrigin.Begin);
                stream.Write(BitConverter.GetBytes(characteristics), 0, sizeof(ushort));
                checksumOffset = header.ChecksumOffset;
                if (header.Checksum == 0)
                {
                    // An unset checksum is valid as-is; leave it alone.
                    checksumOffset = -1;
                }
            }

            if (checksumOffset >= 0 && MapFileAndCheckSumW(path, out _, out var checksum) == 0)
            {
                using (var stream = new FileStream(path, FileMode.Open, FileAccess.Write, FileShare.Read))
                {
                    stream.Seek(checksumOffset, SeekOrigin.Begin);
                    stream.Write(BitConverter.GetBytes(checksum), 0, sizeof(uint));
                }
            }

            return true;
        }

        private static Header ReadHeader(Stream stream)
        {
            var reader = new BinaryReader(stream);
            if (stream.Length < 0x40 || reader.ReadUInt16() != 0x5A4D) // "MZ"
            {
                throw new InvalidDataException("Not a Windows executable.");
            }

            stream.Seek(0x3C, SeekOrigin.Begin);
            var peOffset = reader.ReadInt32();
            if (peOffset <= 0 || peOffset + 24 + 68 > stream.Length)
            {
                throw new InvalidDataException("Invalid PE header offset.");
            }

            stream.Seek(peOffset, SeekOrigin.Begin);
            if (reader.ReadUInt32() != PESignature)
            {
                throw new InvalidDataException("Missing PE signature.");
            }

            var machine = reader.ReadUInt16();       // IMAGE_FILE_HEADER.Machine
            stream.Seek(peOffset + 4 + 18, SeekOrigin.Begin);
            var characteristics = reader.ReadUInt16(); // IMAGE_FILE_HEADER.Characteristics
            var optionalHeader = peOffset + 24;
            var magic = reader.ReadUInt16();
            if (machine != ImageFileMachineI386 || magic != OptionalHeaderMagicPE32)
            {
                throw new InvalidDataException("Not a 32-bit x86 executable.");
            }

            stream.Seek(optionalHeader + 64, SeekOrigin.Begin);
            var checksum = reader.ReadUInt32();        // IMAGE_OPTIONAL_HEADER32.CheckSum

            return new Header
            {
                Characteristics = characteristics,
                CharacteristicsOffset = peOffset + 4 + 18,
                Checksum = checksum,
                ChecksumOffset = optionalHeader + 64,
            };
        }

        [DllImport("imagehlp.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Winapi)]
        private static extern uint MapFileAndCheckSumW(string fileName, out uint headerSum, out uint checkSum);

        private struct Header
        {
            public ushort Characteristics;
            public long CharacteristicsOffset;
            public uint Checksum;
            public long ChecksumOffset;
        }
    }
}
