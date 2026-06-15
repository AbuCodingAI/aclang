### 🛡️ AC+ Core Keyword & Logic Gate Directory


| Keyword / Token | Token Type | Native ASM / OS Equivalent | Meaning & Bare-Metal Execution |
| :--- | :--- | :--- | :--- |
| **`reg`** | `KW_REG` | Registry Declaration | **Hardware Override Flag.** Specifies: "This is a literal hardware register, not a variable." |
| **`cp`** | `KW_CP` | `mov` | Copies a data value or register pointer (similar to bash `cp`). |
| **`move`** | `KW_MOVE` | `movabs` | Moves a massive absolute 64-bit address constant (like your VGA VRAM `0xB8000`). |
| **`offcalc`** | `KW_OFFCALC` | `lea` | Load Effective Address. Calculates structural memory offsets without evaluating memory space. |
| **`is`** | `KW_IS` | `cmp` / `test` | Compares two data items and updates internal CPU flag bits. |
| **`sf`** | `KW_SF` | `subq/addq %rsp` | **Stack Frame block manager.** Carves out dynamic temporary block spaces inside RAM via indent/dedent counters. |
| **`dha`** | `KW_DHA` | `malloc` equivalent | Dynamic Heap Allocation keyword. Claims long-lived, persistence-managed memory frames. |
| **`shiftl`** | `KW_SHIFTL` | `shl` | Shifts binary bits to the left (blistering fast processor multiplication). |
| **`shiftr`** | `KW_SHIFTR` | `shr` | Shifts binary bits to the right (blistering fast processor division). |
| **`<<` / `>>`** | `OP_STREAM` | Direct Hardware Store / Load | Streams raw text bytes or flags directly into targeted register pointer memory addresses. Data flows in the direction the arrows point. |
| **`+`** | `OP_ADD` | `add` | 1:1 hardware addition operation. |
| **`-`** | `OP_SUB` | `sub` | 1:1 hardware subtraction operation. |
| **`*`** | `OP_MUL` | `imul` | 1:1 hardware signed multiplication operation. |
| **`/`** | `OP_DIV` | `idiv` | 1:1 hardware signed division operation. |
| **`&` / `and`** | `OP_AND` | `and` | Bitwise / Logical **AND** gate tracking. |
| **`or`** | `OP_OR` | `or` | Bitwise / Logical **OR** gate tracking. |
| **`\|`** | `OP_XOR` | `xor` | Bitwise / Logical **XOR** (Exclusive OR) gate tracking. |
| **`#`** | `OP_NOT` | `not` | Bitwise / Logical **NOT** inverter gate tracking. |
| **`#\|`** | `OP_XNOR` | `not` + `xor` | Bitwise / Logical **XNOR** (Exclusive NOR) gate tracking. |
| **`/kill`** | `KW_KILL` | `syscall 60` | Direct terminal exit. Destroys the thread state loop instantly for safety. |
| **`specs`** | `KW_SPECS` | Custom System Struct | System state literal. Assigned to variables to capture active hardware specification maps (CPU, GPU, RAM, Kernel). |
| **`ls_drivers`** | `KW_LS_DRIVERS` | `/sys/bus/` Array | Queries and arrays all registered system kernel software modules and interface controllers. |
| **`ls_devices`** | `KW_LS_DEVICES` | Device Tree List | Enumerates physical peripheral units visible in the system control hardware panel. |
| **`ls_usb`** | `KW_LS_USB` | Bus Controller Scan | Polls the hardware hub for active USB serial connection pathways. |
| **`ls_partitions`**| `KW_LS_PARTITIONS` | Storage Node Table | Scans system mount points to layout active hard drive and SSD blocks. |
| **`change section device`**| `KW_DEV_SWITCH` | Driver Hot-Swap | Changes devices within a numbered hardware category array (e.g., change audio device 1->2). |
| **`ptr`** |**`KW_PTR`** | pointer | this is the keyword for pointer, pointer var=x |


