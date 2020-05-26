
pub mod bip_buffer;

#[cfg(test)]
mod tests {
    use crate::bip_buffer::BipBuffer;
    #[test]
    fn bip_buffer_grow() {
        let mut bip_buffer = BipBuffer::new(4096, 0).unwrap();
        assert_eq!(bip_buffer.buffer_size(), 4096);

        assert_eq!(bip_buffer.grow(8192), true);
        assert_eq!(bip_buffer.buffer_size(), 8192);

        assert_eq!(bip_buffer.grow(1024), true);
        assert_eq!(bip_buffer.buffer_size(), 8192);
    }

    #[test]
    fn simple_read_write() {
        let mut bip_buffer = BipBuffer::new(1024, 0).unwrap();

        let input_str = "hello, I am a string!";
        let input = input_str.as_bytes().to_vec();
        let input = input.as_slice();

        let status = bip_buffer.write(input.as_ptr(), input.len());
        assert_eq!(status, input.len() as i32);
        assert_eq!(bip_buffer.is_signaled(), true);

        let mut buffer = [0u8; 4096];
        let status = bip_buffer.read(buffer.as_mut_ptr(), buffer.len());
        assert_eq!(status, input.len() as i32);
        assert_eq!(bip_buffer.is_signaled(), false);

        let output_str = String::from_utf8(buffer[0..status as usize].to_vec()).unwrap();
        assert_eq!(input_str, output_str);
    }
}
