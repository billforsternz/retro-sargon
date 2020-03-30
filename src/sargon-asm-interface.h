// Automatically generated file - C interface to Sargon assembly language
extern "C" {

    // First byte of Sargon data
    extern unsigned char sargon_base_address;

    // Calls to sargon() can set and read back registers
    struct z80_registers
    {
        uint16_t af;    // x86 = lo al, hi flags
        uint16_t hl;    // x86 = bx
        uint16_t bc;    // x86 = cx
        uint16_t de;    // x86 = dx
        uint16_t ix;    // x86 = si
        uint16_t iy;    // x86 = di
    };

    // Call Sargon from C, call selected functions, optionally can set input
    //  registers (and/or inspect returned registers)
    void sargon( int api_command_code, z80_registers *registers=NULL );

    // Sargon calls C, parameters serves double duty - saved registers on the
    //  stack, can optionally be inspected by C program
    void callback( uint32_t edi, uint32_t esi, uint32_t ebp, uint32_t esp,
                   uint32_t ebx, uint32_t edx, uint32_t ecx, uint32_t eax,
                   uint32_t eflags );

    // Data offsets for peeking and poking
    const int TBASE = 0x0100;
    const int BOARDA = 0x0134;
    const int ATKLST = 0x01ac;
    const int PLISTA = 0x01ba;
    const int POSK = 0x01ce;
    const int POSQ = 0x01d0;
    const int SCORE = 0x0200;
    const int PLYIX = 0x0216;
    const int M1 = 0x0300;
    const int M2 = 0x0302;
    const int M3 = 0x0304;
    const int M4 = 0x0306;
    const int T1 = 0x0308;
    const int T2 = 0x030a;
    const int T3 = 0x030c;
    const int INDX1 = 0x030e;
    const int INDX2 = 0x0310;
    const int NPINS = 0x0312;
    const int MLPTRI = 0x0314;
    const int MLPTRJ = 0x0316;
    const int SCRIX = 0x0318;
    const int BESTM = 0x031a;
    const int MLLST = 0x031c;
    const int MLNXT = 0x031e;
    const int KOLOR = 0x0320;
    const int COLOR = 0x0321;
    const int P1 = 0x0322;
    const int P2 = 0x0323;
    const int P3 = 0x0324;
    const int PMATE = 0x0325;
    const int MOVENO = 0x0326;
    const int PLYMAX = 0x0327;
    const int NPLY = 0x0328;
    const int CKFLG = 0x0329;
    const int MATEF = 0x032a;
    const int VALM = 0x032b;
    const int BRDC = 0x032c;
    const int PTSL = 0x032d;
    const int PTSW1 = 0x032e;
    const int PTSW2 = 0x032f;
    const int MTRL = 0x0330;
    const int BC0 = 0x0331;
    const int MV0 = 0x0332;
    const int PTSCK = 0x0333;
    const int BMOVES = 0x0334;
    const int MVEMSG = 0x0340;
    const int MVEMSG_2 = 0x0342;
    const int LINECT = 0x0344;
    const int MLIST = 0x0400;
    const int MLEND = 0xee60;

    // API constants
    const int api_INITBD = 1;
    const int api_ROYALT = 2;
    const int api_CPTRMV = 3;
    const int api_VALMOV = 4;
    const int api_ASNTBI = 5;
    const int api_EXECMV = 6;
};
