#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <zlib.h>
#include <cstring>
#include <chrono>

constexpr size_t BLOCK_SIZE = 1024 * 1024; 

struct Block {
    size_t index;
    std::vector<uint8_t> data;
    std::vector<uint8_t> compressed;
    uLongf compressed_size = 0;
};

class ParallelCompressor {
public:
    ParallelCompressor(const std::string& input, const std::string& output)
        : input_filename(input), output_filename(output) {
        write_mutex = std::make_unique<std::mutex>();
    }

    void run(size_t num_threads) {
        read_input();
        divide_into_blocks();
        compress_blocks_parallel(num_threads);
        write_output();
    }

private:
    std::string input_filename;
    std::string output_filename;
    std::vector<uint8_t> file_data;
    std::vector<Block> blocks;
    std::unique_ptr<std::mutex> write_mutex;

    void read_input() {
        std::ifstream input(input_filename, std::ios::binary);
        if (!input) {
            throw std::runtime_error("No se pudo abrir el archivo de entrada.");
        }

        input.seekg(0, std::ios::end);
        size_t size = input.tellg();
        input.seekg(0, std::ios::beg);

        file_data.resize(size);
        input.read(reinterpret_cast<char*>(file_data.data()), size);
    }

    void divide_into_blocks() {
        size_t total_size = file_data.size();
        size_t num_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        blocks.resize(num_blocks);

        for (size_t i = 0; i < num_blocks; ++i) {
            size_t start = i * BLOCK_SIZE;
            size_t end = std::min(start + BLOCK_SIZE, total_size);

            blocks[i].index = i;
            blocks[i].data.insert(blocks[i].data.begin(),
                                  file_data.begin() + start,
                                  file_data.begin() + end);
        }
    }

    void compress_block(Block& block) {
        uLongf dest_len = compressBound(block.data.size());
        block.compressed.resize(dest_len);

        int result = compress2(block.compressed.data(), &dest_len,
                               block.data.data(), block.data.size(), Z_BEST_COMPRESSION);

        if (result != Z_OK) {
            throw std::runtime_error("Error al comprimir bloque " + std::to_string(block.index));
        }

        block.compressed.resize(dest_len);
        block.compressed_size = dest_len;
    }

    void compress_blocks_parallel(size_t num_threads) {
        auto start_time = std::chrono::high_resolution_clock::now();

        size_t blocks_per_thread = (blocks.size() + num_threads - 1) / num_threads;
        std::vector<std::thread> threads;

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * blocks_per_thread;
            size_t end = std::min(start + blocks_per_thread, blocks.size());

            threads.emplace_back([this, start, end]() {
                for (size_t i = start; i < end; ++i) {
                    compress_block(blocks[i]);
                }
            });
        }

        for (auto& th : threads)
            th.join();

        auto end_time = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end_time - start_time).count();

        std::cout << "Compresión terminada en " << duration << " segundos." << std::endl;
    }

    void write_output() {
        std::ofstream output(output_filename, std::ios::binary);
        if (!output) {
            throw std::runtime_error("No se pudo crear el archivo de salida.");
        }

        for (const auto& block : blocks) {
            uint32_t original_size = block.data.size();
            uint32_t compressed_size = block.compressed_size;

            output.write(reinterpret_cast<const char*>(&original_size), sizeof(original_size));
            output.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
            output.write(reinterpret_cast<const char*>(block.compressed.data()), compressed_size);
        }

        output.close();
        std::cout << "Archivo comprimido guardado como: " << output_filename << std::endl;
    }
};

// --- main program ---
int main() {
    std::cout << "Compresion paralela" << std::endl;
    size_t num_threads;

    std::cout << "Ingrese el número de hilos: ";
    std::cin >> num_threads;

    try {
        ParallelCompressor compressor("paralelismo_teoria.txt", "paralelismo_comprimido.bin");
        compressor.run(num_threads);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
