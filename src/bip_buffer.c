
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * The Bip Buffer - The Circular Buffer with a Twist:
 * http://www.codeproject.com/Articles/3479/The-Bip-Buffer-The-Circular-Buffer-with-a-Twist
 */

typedef struct bip_block BipBlock;
typedef struct bip_buffer BipBuffer;

bool BipBuffer_Grow(BipBuffer* ctx, size_t size);
void BipBuffer_Clear(BipBuffer* ctx);

size_t BipBuffer_UsedSize(BipBuffer* ctx);
size_t BipBuffer_BufferSize(BipBuffer* ctx);

uint8_t* BipBuffer_WriteReserve(BipBuffer* ctx, size_t size);
uint8_t* BipBuffer_WriteTryReserve(BipBuffer* ctx, size_t size, size_t* reserved);
void BipBuffer_WriteCommit(BipBuffer* ctx, size_t size);

uint8_t* BipBuffer_ReadReserve(BipBuffer* ctx, size_t size);
uint8_t* BipBuffer_ReadTryReserve(BipBuffer* ctx, size_t size, size_t* reserved);
void BipBuffer_ReadCommit(BipBuffer* ctx, size_t size);

int BipBuffer_Read(BipBuffer* ctx, uint8_t* data, size_t size);
int BipBuffer_Write(BipBuffer* ctx, const uint8_t* data, size_t size);

void* bip_malloc(size_t size);
void bip_free(void* ptr);

void* bip_malloc(size_t size)
{
	return malloc(size);
}

void bip_free(void* ptr)
{
	free(ptr);
}

void* bip_memcpy(void* dst, void* src, size_t size)
{
	return memcpy(dst, src, size);
}

#define BIP_BUFFER_PAGE_SIZE    4096

struct bip_block
{
	size_t index;
	size_t size;
};

struct bip_buffer
{
	size_t size;
	uint8_t* buffer;
	size_t pageSize;
	BipBlock blockA;
	BipBlock blockB;
	BipBlock readR;
	BipBlock writeR;
	bool signaled;
};

void BipBuffer_Lock(BipBuffer* ctx)
{
    // TODO: lock bip buffer
}

void BipBuffer_Unlock(BipBuffer* ctx)
{
    // TODO: unlock bip buffer
}

void BipBuffer_SetEvent(BipBuffer* ctx)
{
	ctx->signaled = true; // set event to signaled state
}

void BipBuffer_ResetEvent(BipBuffer* ctx)
{
	ctx->signaled = false; // set event to non-signaled state
}

void BipBlock_Clear(BipBlock* ctx)
{
    ctx->index = 0;
    ctx->size = 0;
}

void BipBlock_Copy(BipBlock* dst, BipBlock* src)
{
    dst->index = src->index;
    dst->size = src->size;
}

void BipBuffer_Clear(BipBuffer* ctx)
{
	BipBlock_Clear(&ctx->blockA);
	BipBlock_Clear(&ctx->blockB);
	BipBlock_Clear(&ctx->readR);
	BipBlock_Clear(&ctx->writeR);
}

bool BipBuffer_Grow(BipBuffer* ctx, size_t size)
{
	uint8_t* block;
	uint8_t* buffer;
	size_t blockSize = 0;
	size_t commitSize = 0;

	size += size % ctx->pageSize;

	if (size <= ctx->size)
		return true;

	buffer = (uint8_t*) bip_malloc(size);

	if (!buffer)
		return false;

	block = BipBuffer_ReadTryReserve(ctx, 0, &blockSize);

	if (block)
	{
		memcpy(&buffer[commitSize], block, blockSize);
		BipBuffer_ReadCommit(ctx, blockSize);
		commitSize += blockSize;
	}

	block = BipBuffer_ReadTryReserve(ctx, 0, &blockSize);

	if (block)
	{
		memcpy(&buffer[commitSize], block, blockSize);
		BipBuffer_ReadCommit(ctx, blockSize);
		commitSize += blockSize;
	}

	BipBuffer_Clear(ctx);

	bip_free(ctx->buffer);
	ctx->buffer = buffer;
	ctx->size = size;

	ctx->blockA.index = 0;
	ctx->blockA.size = commitSize;

	return true;
}

size_t BipBuffer_UsedSize(BipBuffer* ctx)
{
	return ctx->blockA.size + ctx->blockB.size;
}

size_t BipBuffer_BufferSize(BipBuffer* ctx)
{
	return ctx->size;
}

uint8_t* BipBuffer_WriteTryReserve(BipBuffer* ctx, size_t size, size_t* reserved)
{
	size_t reservable;

	if (!reserved)
		return NULL;

	if (!ctx->blockB.size)
	{
		/* block B does not exist */

		reservable = ctx->size - ctx->blockA.index - ctx->blockA.size; /* space after block A */

		if (reservable >= ctx->blockA.index)
		{
			if (reservable == 0)
				return NULL;

			if (size < reservable)
				reservable = size;

			ctx->writeR.size = reservable;
			*reserved = reservable;

			ctx->writeR.index = ctx->blockA.index + ctx->blockA.size;
			return &ctx->buffer[ctx->writeR.index];
		}

		if (ctx->blockA.index == 0)
			return NULL;

		if (ctx->blockA.index < size)
			size = ctx->blockA.index;

		ctx->writeR.size = size;
		*reserved = size;

		ctx->writeR.index = 0;
		return ctx->buffer;
	}

	/* block B exists */

	reservable = ctx->blockA.index - ctx->blockB.index - ctx->blockB.size; /* space after block B */

	if (size < reservable)
		reservable = size;

	if (reservable == 0)
		return NULL;

	ctx->writeR.size = reservable;
	*reserved = reservable;

	ctx->writeR.index = ctx->blockB.index + ctx->blockB.size;
	return &ctx->buffer[ctx->writeR.index];
}

uint8_t* BipBuffer_WriteReserve(BipBuffer* ctx, size_t size)
{
	uint8_t* block = NULL;
	size_t reserved = 0;

	block = BipBuffer_WriteTryReserve(ctx, size, &reserved);

	if (reserved == size)
		return block;

	if (!BipBuffer_Grow(ctx, size))
		return NULL;

	block = BipBuffer_WriteTryReserve(ctx, size, &reserved);

	return block;
}

void BipBuffer_WriteCommit(BipBuffer* ctx, size_t size)
{
	int oldSize = 0;
	int newSize = 0;

	oldSize = (int) BipBuffer_UsedSize(ctx);

	if (size == 0)
	{
		BipBlock_Clear(&ctx->writeR);
		goto exit;
	}

	if (size > ctx->writeR.size)
		size = ctx->writeR.size;

	if ((ctx->blockA.size == 0) && (ctx->blockB.size == 0))
	{
		ctx->blockA.index = ctx->writeR.index;
		ctx->blockA.size = size;
		BipBlock_Clear(&ctx->writeR);
		goto exit;
	}

	if (ctx->writeR.index == (ctx->blockA.size + ctx->blockA.index))
		ctx->blockA.size += size;
	else
		ctx->blockB.size += size;

	BipBlock_Clear(&ctx->writeR);

exit:
	newSize = (int) BipBuffer_UsedSize(ctx);

	if ((oldSize <= 0) && (newSize > 0))
		BipBuffer_SetEvent(ctx);
}

int BipBuffer_Write(BipBuffer* ctx, const uint8_t* data, size_t size)
{
	int status = 0;
	int oldSize = 0;
	int newSize = 0;
	size_t writeSize = 0;
	size_t blockSize = 0;
	uint8_t* block = NULL;

	if (!ctx)
		return -1;

    BipBuffer_Lock(ctx);

	oldSize = (int) BipBuffer_UsedSize(ctx);

	block = BipBuffer_WriteReserve(ctx, size);

	if (!block)
	{
		status = -1;
		goto exit;
	}

	block = BipBuffer_WriteTryReserve(ctx, size - status, &blockSize);

	if (block)
	{
		writeSize = size - status;

		if (writeSize > blockSize)
			writeSize = blockSize;

		memcpy(block, &data[status], writeSize);
		BipBuffer_WriteCommit(ctx, writeSize);
		status += (int) writeSize;

		if ((status == size) || (writeSize < blockSize))
			goto exit;
	}

	block = BipBuffer_WriteTryReserve(ctx, size - status, &blockSize);

	if (block)
	{
		writeSize = size - status;

		if (writeSize > blockSize)
			writeSize = blockSize;

		memcpy(block, &data[status], writeSize);
		BipBuffer_WriteCommit(ctx, writeSize);
		status += (int) writeSize;

		if ((status == size) || (writeSize < blockSize))
			goto exit;
	}

exit:
	newSize = (int) BipBuffer_UsedSize(ctx);

	if ((oldSize <= 0) && (newSize > 0))
		BipBuffer_SetEvent(ctx);

    BipBuffer_Unlock(ctx);

	return status;
}

uint8_t* BipBuffer_ReadTryReserve(BipBuffer* ctx, size_t size, size_t* reserved)
{
	size_t reservable = 0;

	if (!reserved)
		return NULL;

	if (ctx->blockA.size == 0)
	{
		*reserved = 0;
		return NULL;
	}

	reservable = ctx->blockA.size;

	if (size && (reservable > size))
		reservable = size;

	*reserved = reservable;
	return &ctx->buffer[ctx->blockA.index];
}

uint8_t* BipBuffer_ReadReserve(BipBuffer* ctx, size_t size)
{
	uint8_t* block = NULL;
	size_t reserved = 0;

	if (BipBuffer_UsedSize(ctx) < size)
		return NULL;

	block = BipBuffer_ReadTryReserve(ctx, size, &reserved);

	if (reserved == size)
		return block;

	if (!BipBuffer_Grow(ctx, ctx->size + 1))
		return NULL;

	block = BipBuffer_ReadTryReserve(ctx, size, &reserved);

	if (reserved != size)
		return NULL;

	return block;
}

void BipBuffer_ReadCommit(BipBuffer* ctx, size_t size)
{
	int oldSize = 0;
	int newSize = 0;

	if (!ctx)
		return;

	oldSize = (int) BipBuffer_UsedSize(ctx);

	if (size >= ctx->blockA.size)
	{
		BipBlock_Copy(&ctx->blockA, &ctx->blockB);
		BipBlock_Clear(&ctx->blockB);
	}
	else
	{
		ctx->blockA.size -= size;
		ctx->blockA.index += size;
	}

	newSize = (int) BipBuffer_UsedSize(ctx);

	if ((oldSize > 0) && (newSize <= 0))
		BipBuffer_ResetEvent(ctx);
}

int BipBuffer_Read(BipBuffer* ctx, uint8_t* data, size_t size)
{
	int status = 0;
	int oldSize = 0;
	int newSize = 0;
	size_t readSize = 0;
	size_t blockSize = 0;
	uint8_t* block = NULL;

	if (!ctx)
		return -1;

    BipBuffer_Lock(ctx);

	oldSize = (int) BipBuffer_UsedSize(ctx);

	block = BipBuffer_ReadTryReserve(ctx, 0, &blockSize);

	if (block)
	{
		readSize = size - status;

		if (readSize > blockSize)
			readSize = blockSize;

		memcpy(&data[status], block, readSize);
		BipBuffer_ReadCommit(ctx, readSize);
		status += (int) readSize;

		if ((status == size) || (readSize < blockSize))
			goto exit;
	}

	block = BipBuffer_ReadTryReserve(ctx, 0, &blockSize);

	if (block)
	{
		readSize = size - status;

		if (readSize > blockSize)
			readSize = blockSize;

		memcpy(&data[status], block, readSize);
		BipBuffer_ReadCommit(ctx, readSize);
		status += (int) readSize;

		if ((status == size) || (readSize < blockSize))
			goto exit;
	}

exit:
	newSize = (int) BipBuffer_UsedSize(ctx);

	if ((oldSize > 0) && (newSize <= 0))
		BipBuffer_ResetEvent(ctx);

    BipBuffer_Unlock(ctx);

	return status;
}
