import cpu.iss.riscv
import memory.memory
import vp.clock_domain
import interco.router
import utils.loader.loader
import gvsoc.systree
import gvsoc.runner
import mm_dma
import mm_accel


GAPY_TARGET = True

class Soc(gvsoc.systree.Component):

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        # Parse the arguments to get the path to the binary to be loaded
        [args, __] = parser.parse_known_args()

        binary = args.binary

        # Main interconnect
        ico = interco.router.Router(self, 'ico')

        # Custom DMA
        dma_comp = mm_dma.mm_dma(self, 'mm_dma')
        ico.o_MAP(dma_comp.i_INPUT(), 'mm_dma', base=0x20000000, size=0x00001000, rm_base=True)
        dma_comp.o_OUTPUT ( ico.i_INPUT     ())

        # Custom accel
        accel_comp = mm_accel.mm_accel(self, 'mm_accel')
        ico.o_MAP(accel_comp.i_INPUT(), 'mm_accel', base=0x30000000, size=0x00001000, rm_base=True)

        # Main memory
        mem = memory.memory.Memory(self, 'mem', size=0x00100000)
        # The memory needs to be connected with a mpping. The rm_base is used to substract
        # the global address to the requests address so that the memory only gets a local offset.
        ico.o_MAP(mem.i_INPUT(), 'mem', base=0x00000000, size=0x00100000, rm_base=True)

        # Instantiates the main core and connect fetch and data to the interconnect
        host0 = cpu.iss.riscv.Riscv(self, 'host0', isa='rv64imafdc', core_id=0)
        host0.o_FETCH     ( ico.i_INPUT     ())
        host0.o_DATA      ( ico.i_INPUT     ())

        # host1 = cpu.iss.riscv.Riscv(self, 'host1', isa='rv64imafdc', core_id=1)
        # host1.o_FETCH     ( ico.i_INPUT     ())
        # host1.o_DATA      ( ico.i_INPUT     ())

        # Finally connect an ELF loader, which will execute first and will then
        # send to the core the boot address and notify him he can start
        loader = utils.loader.loader.ElfLoader(self, 'loader', binary=binary)
        loader.o_OUT     ( ico.i_INPUT     ())
        loader.o_START   ( host0.i_FETCHEN  ())
        loader.o_ENTRY   ( host0.i_ENTRY    ())
        # loader.o_START   ( host1.i_FETCHEN  ())
        # loader.o_ENTRY   ( host1.i_ENTRY    ())



# This is a wrapping component of the real one in order to connect a clock generator to it
# so that it automatically propagate to other components
class Rv64(gvsoc.systree.Component):

    def __init__(self, parent, name, parser, options):

        super().__init__(parent, name, options=options)

        clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100000000)
        soc = Soc(self, 'soc', parser)
        clock.o_CLOCK    ( soc.i_CLOCK     ())




# This is the top target that gapy will instantiate
class Target(gvsoc.runner.Target):

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=Rv64, description="RV64 virtual board")

