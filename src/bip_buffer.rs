
pub type CBipBuffer = usize;

#[allow(dead_code)]
extern "C" {
    fn BipBuffer_Grow(ctx: CBipBuffer, size: usize) -> bool;
    
    fn BipBuffer_WriteReserve(ctx: CBipBuffer, size: usize) -> *mut u8;
    fn BipBuffer_WriteTryReserve(ctx: CBipBuffer, size: usize, reserved: &usize) -> *mut u8;
    fn BipBuffer_WriteCommit(ctx: CBipBuffer, size: usize);

    fn BipBuffer_ReadReserve(ctx: CBipBuffer, size: usize) -> *const u8;
    fn BipBuffer_ReadTryReserve(ctx: CBipBuffer, size: usize, reserved: &usize) -> *const u8;
    fn BipBuffer_ReadCommit(ctx: CBipBuffer, size: usize);

    fn BipBuffer_Read(ctx: CBipBuffer, data: *mut u8, size: usize) -> i32;
    fn BipBuffer_Write(ctx: CBipBuffer, data: *const u8, size: usize) -> i32;

    fn bip_malloc(size: usize) -> usize;
    fn bip_free(ptr: usize);

    fn bip_memcpy(dst: usize, src: usize, size: usize) -> usize;
}

#[repr(C)]
pub struct BipBlock {
    pub index: usize,
    pub size: usize,
}

impl Default for BipBlock {
    fn default() -> Self {
        BipBlock {
            index: 0,
            size: 0,
        }
    }
}

impl BipBlock {
    pub fn clear(&mut self) {
        self.index = 0;
        self.size = 0;
    }

    pub fn copy(dst: &mut Self, src: &Self) {
        dst.index = src.index;
        dst.size = src.size;
    }
}

#[repr(C)]
pub struct BipBuffer {
    pub size: usize,
    pub buffer: *mut u8,
    pub page_size: usize,
    pub block_a: BipBlock,
    pub block_b: BipBlock,
    pub read_r: BipBlock,
    pub write_r: BipBlock,
}

const BIP_BUFFER_PAGE_SIZE: usize = 4096;

impl Default for BipBuffer {
    fn default() -> Self {
        BipBuffer {
            size: 0,
            buffer: 0 as *mut u8,
            page_size: 0,
            block_a: BipBlock::default(),
            block_b: BipBlock::default(),
            read_r: BipBlock::default(),
            write_r: BipBlock::default(),
        }
    }
}

impl BipBuffer {
    fn init(&mut self) -> bool {
        if self.page_size == 0 {
            self.page_size = BIP_BUFFER_PAGE_SIZE;
        }

        self.alloc_buffer(self.size)
    }

    fn uninit(&mut self) {
        self.free_buffer();
    }

    pub fn new(buffer_size: usize, page_size: usize) -> Option<Self> {
        let mut bip_buffer = BipBuffer::default();
        bip_buffer.size = buffer_size;
        bip_buffer.page_size = page_size;

        if bip_buffer.init() {
            Some(bip_buffer)
        } else {
            None
        }
    }

    pub fn clear(&mut self) {
        self.block_a.clear();
        self.block_b.clear();
        self.read_r.clear();
        self.write_r.clear();
    }

    pub fn alloc_buffer(&mut self, size: usize) -> bool {
        if size < 1 {
            return false;
        }

        let size = size + size % self.page_size;

        unsafe {
            self.buffer = bip_malloc(size) as *mut u8;
        }

        if self.buffer.is_null() {
            return false;
        }

        self.size = size;

        return true;
    }

    pub fn grow(&mut self, size: usize) -> bool {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_Grow(ctx, size)
        }
    }

    pub fn free_buffer(&mut self) {
        if !self.buffer.is_null() {
            unsafe {
                bip_free(self.buffer as usize);
            }
            self.buffer = 0 as *mut u8;
        }

        self.clear();
    }

    pub fn used_size(&self) -> usize {
        self.block_a.size + self.block_b.size
    }

    pub fn buffer_size(&self) -> usize {
        self.size
    }

    pub fn write_reserve(&mut self, size: usize) -> *mut u8 {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_WriteReserve(ctx, size)
        }
    }

    pub fn write_try_reserve(&mut self, size: usize, reserved: &usize) -> *mut u8 {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_WriteTryReserve(ctx, size, reserved)
        }
    }

    pub fn write_commit(&mut self, size: usize) {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_WriteCommit(ctx, size);
        }
    }

    pub fn write(&self, data: *const u8, size: usize) -> i32 {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_Write(ctx, data, size)
        }
    }

    pub fn read_reserve(&mut self, size: usize) -> *const u8 {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_ReadReserve(ctx, size)
        }
    }

    pub fn read_try_reserve(&mut self, size: usize, reserved: &usize) -> *const u8 {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_ReadTryReserve(ctx, size, reserved)
        }
    }

    pub fn read_commit(&mut self, size: usize) {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_ReadCommit(ctx, size);
        }
    }

    pub fn read(&self, data: *mut u8, size: usize) -> i32 {
        unsafe {
            let ctx = self as *const Self as CBipBuffer;
            BipBuffer_Read(ctx, data, size)
        }
    }
}
 impl Drop for BipBuffer {
     fn drop(&mut self) {
         self.uninit();
     }
 }
