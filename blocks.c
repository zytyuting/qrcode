#include "blocks.h"

#include <stdlib.h>
#include <stdio.h>

#include "modules.h"
#include "correction.h"

static void get_block(scanner_t* scanner);

static void get_block(scanner_t* scanner)
{
	// get block information
	static const byte blocks[160][7] =
	{
#include "blocksizes.h"
	};
	const byte* b = blocks[4*(scanner->v-1) + scanner->c];

	// current block
	size_t cur = scanner->block_cur;

	// find the data size of the current block
	size_t ndata = cur < b[0] ? b[2] : b[5];
	scanner->block_dataw = ndata;

	// rewind to start of symbol
	scanner->i = scanner->s-1;
	scanner->j = scanner->s-1;

	// NOTE: in a symbol, all the blocks have the same number of error
	//       correction codewords but the first series of blocks can
	//       have one data codeword less

	byte nblocks = b[0]+ b[3];

	// BEGIN read data
	// the next section handles the inverleaving of data codewords

	// handling the minimal number of codewords
	// n is either ndata-1 (first blocks) or ndata-2 (last ones)
	for (size_t i = 0; i < b[2]; i++)
	{
		skip_bits(scanner, cur * 8);
		scanner->block_data[i] = get_codeword(scanner);
		skip_bits(scanner, (nblocks-cur-1) * 8);
	}

	// interleaving specific to the second series of blocks
	if (b[2] == ndata) // first kind of block
	{
		skip_bits(scanner, b[3] * 8);
	}
	else // second kind
	{
		skip_bits(scanner, (cur-b[0]) * 8);
		scanner->block_data[ndata-1] = get_codeword(scanner);
		skip_bits(scanner, (nblocks-cur-1) * 8);

	}

	// END read data


	// the module pointers is now at the end of all the data and at
	// the beginning of the interleaved error correction codewords


	// BEGIN read correction
	// this section handles the interleaving of error correction codewords

	size_t n = b[1] - b[2]; // same for both types of blocks
	for (size_t i = 0; i < n; i++)
	{
		skip_bits(scanner, cur * 8);
		scanner->block_data[ndata+i] = get_codeword(scanner);
		skip_bits(scanner, (nblocks-cur-1) * 8);
	}

	// END read correction


	// apply Reed-Solomon error correction
	if (rs_correction(ndata+n, scanner->block_data, n) != 0)
	{
		fprintf(stderr, "Could not correct errors\n");
		exit(1);
	}

	scanner->block_cur = cur+1;
	scanner->block_curbyte = 0;
	scanner->block_curbit = 0;
}

unsigned int get_bits(scanner_t* scanner, size_t n)
{
	if (!scanner->block_cur)
		get_block(scanner);

	// this buffer handling is an abomination
	unsigned int res = 0;
	while (n--)
	{
		if (scanner->block_curbyte >= scanner->block_dataw)
			get_block(scanner);

		size_t B = scanner->block_curbyte;
		size_t b = scanner->block_curbit;
		res *= 2;
		res += (scanner->block_data[B] >> (7-b)) & 1;

		scanner->block_curbit++;
		if (scanner->block_curbit >= 8)
		{
			scanner->block_curbyte++;
			scanner->block_curbit = 0;
		}
	}
	return res;
}
