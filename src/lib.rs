
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
}
