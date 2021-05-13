#include <conio.h>  
#include <stdio.h>
#include <signal.h> //btcdbcb
#include <stdint.h>
#include <Windows.h>



HANDLE hStdin = INVALID_HANDLE_VALUE;



// 16 Opcodes (3 are not implemented)
enum
{
    OPCODE_BR = 0, //Branch (Not implemented)
    OPCODE_ADD,    //Add
    OPCODE_LD,     //Load
    OPCODE_ST,     //Store
    OPCODE_JSR,    //Jump Register
    OPCODE_AND,    //Bitwise AND
    OPCODE_LDR,    //Load Register (Not implemented)
    OPCODE_STR,    //Store register (Not implemented)
    OPCODE_RTI,    //Unused
    OPCODE_NOT,    //Bitwise NOT
    OPCODE_LDI,    // Load indirect 
    OPCODE_STI,    // Store indirect
    OPCODE_JMP,    // Jump
    OPCODE_RES,    // Reserved (unused) 
    OPCODE_LEA,    // Load effective address 
    OPCODE_TRAP    // Execute trap routine 
};


//Registers

enum
{
    REGISTER_R0 = 0,
    REGISTER_R1,
    REGISTER_R2,
    REGISTER_R3,
    REGISTER_R4,
    REGISTER_R5,
    REGISTER_R6,
    REGISTER_R7,
    REGISTER_PC,
    REGISTER_COND,
    REGISTER_COUNT
};


//NPZ Flags

enum
{
    positive_flag = 1 << 0, 
    zero_flag = 1 << 1, 
    negative_flag = 1 << 2, 
};




// Memory mapped registers 
enum
{
    KBSR = 0xFE00, //keyboard status
    KBDR = 0xFE02  // keyboard data 
};

/* TRAP Codes */
enum
{
    GETC = 0x20,  // get character from keyboard
    T_OUT = 0x21, //Output a single character
    PUTS = 0x22,  //Output a string
    T_IN = 0x23,  //get character from keyboard and echo on the terminal
    PUTSP = 0x24, //Output a byte string (Not implemented)
    HALT = 0x25   //Halt the process
};


//lc3 memory (65536 memory locations each 16-bit)
uint16_t memory[UINT16_MAX];

//lc3 registers
uint16_t reg[REGISTER_COUNT];


//sign extending n to 16-bit value 
uint16_t sign_extention(uint16_t n, int bits)
{
    if ((n >> (bits - 1)) & 1) {
        n |= (0xFFFF << bits);
    }
    return n;
}

//changing big endian to little endian 

uint16_t swap(uint16_t n)
{
    return (n << 8) | (n >> 8);
}


//updating the NPZ flags
void flagUpdate(uint16_t n)
{
    if (reg[n] == 0)
    {
        reg[REGISTER_COND] = zero_flag;
    }
    else if (reg[n] >> 15) //1 in left-most bit indicates a negative value
    {
        reg[REGISTER_COND] = negative_flag;
    }
    else
    {
        reg[REGISTER_COND] = positive_flag;
    }
}


//read image file
void r_image(FILE* file)
{
    //the origin points to where in the memory we must start storing the image 
    uint16_t ORIG;
    fread(&ORIG, sizeof(ORIG), 1, file);
    

   ORIG = swap(ORIG);
    uint16_t maxr = UINT16_MAX - ORIG;
    uint16_t* x = memory + ORIG;
    size_t rd = fread(x, sizeof(uint16_t), maxr, file);

    //swap to little endian
    while (rd-- > 0)
    {
        *x = swap(*x);
        ++x;
    }
}

//read image file
int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    r_image(file);
    fclose(file);
    return 1;
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}



// write/store a value to the memory
void memory_write(uint16_t address, uint16_t value)
{
    memory[address] = value;
}

//read a value form memory and return
uint16_t memory_read(uint16_t address)
{
    return memory[address];
}


DWORD fdwMode, fdwOldMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); 
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  
            ^ ENABLE_LINE_INPUT; 
    SetConsoleMode(hStdin, fdwMode); 
    FlushConsoleInputBuffer(hStdin); 
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOldMode);
}


void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}



//main method
int main(int argc, const char* argv[])
{
    
    if (argc < 2)
    {
        // if user doesn't enter the path for the assembled image file then display this and exit from the program 
        printf("please add the path to image file => lc3 [PATH]\n");
        exit(2);
    }
	
		//  calling the read_image function that reads the contents of the file at given path and stores in memory one by one
    for (int v = 1; v < argc; v++)
    {
        if (!read_image(argv[v]))
        {
            printf("failed to load image: %s\n", argv[v]);
            exit(1);
        }
    }

    
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();




	// program always starts at x3000 location in lc3
    reg[REGISTER_PC] = 0x3000;

	//running indicates the state of the process
    int running = 1;
    while (running)
    {
        // fetching the instruction from memory at location of Program Counter and updating it by adding 1
        uint16_t INSTRUCTION = memory_read(reg[REGISTER_PC]++);
        
        // Getting the opcode from the fetched Instruction
        uint16_t OPCODE = INSTRUCTION >> 12;

        switch (OPCODE)
        {
            case OPCODE_ADD:
//              ADD 
                {
                   //r0 is the destionation register
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                   
                   //r1 is the first operand 
                    uint16_t r1 = (INSTRUCTION >> 6) & 0x7;
                    
                    //addressing_mode = 1 means direct and 0 means indirect
                    uint16_t addressing_mode = (INSTRUCTION >> 5) & 0x1;
                
                    if (addressing_mode)
                    {
                        uint16_t immediate_value = sign_extention(INSTRUCTION & 0x1F, 5);
                        reg[r0] = reg[r1] + immediate_value;
                    }
                    else
                    {
                        uint16_t r2 = INSTRUCTION & 0x7;
                        reg[r0] = reg[r1] + reg[r2];
                    }
                
                    flagUpdate(r0);
                }

                break;
            case OPCODE_AND:
//          		Binary AND
                {
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                    uint16_t r1 = (INSTRUCTION >> 6) & 0x7;
                    uint16_t addressing_mode = (INSTRUCTION >> 5) & 0x1;
                
                    if (addressing_mode)
                    {
                        uint16_t immediate_value = sign_extention(INSTRUCTION & 0x1F, 5);
                        reg[r0] = reg[r1] & immediate_value;
                    }
                    else
                    {
                        uint16_t r2 = INSTRUCTION & 0x7;
                        reg[r0] = reg[r1] & reg[r2];
                    }
                    flagUpdate(r0);
                }

                break;
            case OPCODE_NOT:
//         		Binary NOT 
                {
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                    uint16_t r1 = (INSTRUCTION >> 6) & 0x7;
                
                    reg[r0] = ~reg[r1];
                    flagUpdate(r0);
                }

                break;
                
            case OPCODE_BR:
//               Branch 
                {
                    uint16_t pc_offset = sign_extention(INSTRUCTION & 0x1FF, 9);
                    uint16_t cond_flag = (INSTRUCTION >> 9) & 0x7;
                    if (cond_flag & reg[REGISTER_COND])
                    {
                        reg[REGISTER_PC] += pc_offset;
                    }
                }

                break;

            case OPCODE_JMP:
//                Jump
                {
                   
                    uint16_t r1 = (INSTRUCTION >> 6) & 0x7;
                    reg[REGISTER_PC] = reg[r1];
                }

                break;
            case OPCODE_JSR:
//                Jump Register
                {
                    uint16_t long_flag = (INSTRUCTION >> 11) & 1;
                    reg[REGISTER_R7] = reg[REGISTER_PC];
                    if (long_flag)
                    {
                        uint16_t long_pc_offset = sign_extention(INSTRUCTION & 0x7FF, 11);
                        reg[REGISTER_PC] += long_pc_offset;  /* JSR */
                    }
                    else
                    {
                        uint16_t r1 = (INSTRUCTION >> 6) & 0x7;
                        reg[REGISTER_PC] = reg[r1]; /* JSRR */
                    }
                    break;
                }

                break;
            case OPCODE_LD:
//               Load
                {
                	// r0 is Destination Register
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                    uint16_t pc_offset = sign_extention(INSTRUCTION & 0x1FF, 9);
                    
                    /* Adding pc offset to the program counter, looking at that address in memory and retrieveing the contents */
                    reg[r0] = memory_read(reg[REGISTER_PC] + pc_offset);
                    flagUpdate(r0);
                }

                break;
            case OPCODE_LDI:
//                Load Indirect
                {
                    // r0 is Destination Register
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                    
                    uint16_t pc_offset = sign_extention(INSTRUCTION & 0x1FF, 9);
                    /* Adding pc offset to the program counter, looking at that address in memory to get the final address */
                    reg[r0] = memory_read(memory_read(reg[REGISTER_PC] + pc_offset));
                    flagUpdate(r0);
                }

                break;

            case OPCODE_LEA:
//                Load effective address
                {
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                    uint16_t pc_offset = sign_extention(INSTRUCTION & 0x1FF, 9);
                    reg[r0] = reg[REGISTER_PC] + pc_offset;
                    flagUpdate(r0);
                }

                break;
            case OPCODE_ST:
//               Store 
                {
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                    uint16_t pc_offset = sign_extention(INSTRUCTION & 0x1FF, 9);
                    memory_write(reg[REGISTER_PC] + pc_offset, reg[r0]);
                }

                break;
            case OPCODE_STI:
//                Store Indirect
                {
                    uint16_t r0 = (INSTRUCTION >> 9) & 0x7;
                    uint16_t pc_offset = sign_extention(INSTRUCTION & 0x1FF, 9);
                    memory_write(memory_read(reg[REGISTER_PC] + pc_offset), reg[r0]);
                }

                break;

            case OPCODE_TRAP:
//                Trap Routines
                switch (INSTRUCTION & 0xFF)
                {
                    case GETC:
                     
                     	printf("\nEnter a character : ");
                        reg[REGISTER_R0] = (uint16_t)getchar();

                        break;
                    case T_OUT:
                        
                        printf("\n");
                        putc((char)reg[REGISTER_R0], stdout);
                        fflush(stdout);

                        break;
                    case PUTS:
                     
                        {
                            
                            uint16_t* char_pointer = memory + reg[REGISTER_R0];
                            while (*char_pointer)
                            {
                                putc((char)*char_pointer, stdout);
                                ++char_pointer;
                            }
                            fflush(stdout);
                        }

                        break;
                    case T_IN:
                        
                        {
                            printf("Enter a character: ");
                            char c = getchar();
                            putc(c, stdout);
                            printf("\n");
                            reg[REGISTER_R0] = (uint16_t)c;
                        }

                        break;

                    case HALT:
                        
                        puts("\nprogram terminated");
                        fflush(stdout);
                        running = 0;

                        break;
                }

                break;

            default:
//               INVALID OPCODE 

                break;
        }
    }
//    shutdown
    restore_input_buffering();

}


