# Art-Net output

Art-Net(TM) Designed by and Copyright Artistic Licence.

LJ4EVR sends Art-Net 4 ArtDmx beside sACN (design D14, note 08 s9; gates W5-ART-1..7 of `docs/w5-gates.md`). LightJockey
2 had no network output of its own (note 05 item 23), so nothing here copies LJ2 behaviour: the protocol choice, its
captions and its defaults are ours and tagged PROPOSAL.

## Choosing Art-Net for a link

Setup > Hardware Setup, DMX Hardware page. Each link row has the sACN universe, a protocol box (`sACN` / `Art-Net`) and
an Art-Net `Port-Address` (1..32767; 0 is refused because the specification deprecates it). The default is sACN on
every link with Port-Address = link number. OK applies the record (`LJ_CMD_SACN_CFG`, the engine thread re-opens both
senders) and writes `Hardware.Ini [ARTNET]` (`docs/file-formats.md`). A link set to Art-Net leaves sACN: its universe
gets three Stream_Terminated packets and is no longer sent or announced.

The status line under the page shows, after the sACN part, `Art-Net <n> pkt/s, <k> nodes`, or `Art-Net off: UDP 6454
in use` when another Art-Net program already holds the port. Art-Net failing never stops sACN.

## What goes on the wire (`src/io/lj_artnet.c`)

| Packet | When | To |
|---|---|---|
| ArtPoll (OpCode 0x2000, ProtVer 14, 22 bytes, Flags 0, Oem 0x00FF) | on open, then every 2.5 s | the chosen interface's directed broadcast (`address OR NOT mask`); never 255.255.255.255 |
| ArtDmx (OpCode 0x5000, ProtVer 14, 530 bytes) | on a change of the link's 512 slots, else every 900 ms; at most one per 22.7 ms (44 Hz) unless "Unlock packet rate" | unicast to each node whose ArtPollReply lists the link's Port-Address as an output; nothing at all when no node does |

ArtDmx fields: Sequence 1..255 per link (0 never sent), Physical = link - 1 (PROPOSAL), SubUni / Net from the
Port-Address, Length 512 high byte first, the slots without a START code. One packet per host and link even when a node
answers with several BindIndex pages. LJ4EVR is a controller only: it never answers ArtPoll and sends no ArtSync, ArtNzs
or ArtAddress.

The subscriber table holds 32 rows (PROPOSAL), one per source address and BindIndex, filled from NetSwitch, SubSwitch,
PortTypes and SwOut of each ArtPollReply; a row is dropped at the poll after 3 polls without a reply (7.5 to 10 s after its last reply, PROPOSAL). Replies are
filed under the datagram's source address, never the IP field inside the packet. Packets from our own address are
ignored.

## The socket

One UDP socket bound to the chosen interface address, port 6454, with SO_BROADCAST, owned by the engine thread. It is
open only while a link is set to Art-Net or DMX-in uses Art-Net (derived from `[DMXIN]` at run time, never saved,
ME-178); with every link on sACN and DMX-in off nothing listens on 6454. With the interface left at 0.0.0.0 the socket binds every interface and polls the first
interface that is up. Every datagram read is hostile input: the readers check the ID, the OpCode, ProtVer >= 14 for
ArtDmx (an ArtPoll from another controller is not read: only ArtPollReply and ArtDmx are), the length before each
field, ArtDmx Length even and 2..512 and within the datagram, and copy names bounded. An ArtPollReply is accepted only
from the bound interface's subnet (a loopback address counts as 127/8), and one source address holds at most 4
subscriber rows (`LJ_ARTNET_ROWS_PER_IP`, PROPOSAL rig W5-ART-2), so a forged reply from elsewhere cannot turn
LJ4EVR into a unicast reflector or fill the table (ME-176). An ArtDmx for the input Port-Address from another host on
that subnet goes whole to DMX-in (`lj_dmxin_post`); from off the subnet it is dropped (ME-177).

## OemCode

Art-Net asks every implementer to apply to Artistic Licence for an OemCode. Until one is issued LJ4EVR sends 0x00FF
(OemUnknown) in ArtPoll and EstaMan 0 (PROPOSAL rig W5-ART-7: the applied-for value replaces `LJ_ARTNET_OEM` in
`src/io/lj_artnet.h`).

## Checks

- `lj4evr_tests test_artnet` (CTest `test_artnet`): the builders against `tests/vectors.h` (written by
  `tools/gen_vectors.py` from its own struct.pack builder), the readers, `[ARTNET]`, the Hardware Setup rows, discovery
  and expiry, 400 changes = 400 packets, keep-alives, the 44 Hz cap, no subscriber = no packet, a pre-bound 6454, and a
  loopback wire check (our socket on 127.0.0.2, a node on 127.0.0.1; SKIP where 127.0.0.2 cannot be bound).
- `lj4evr_tests fuzz_artnet` (CTest) and `tools/fuzz/fuzz_artnet.c` (libFuzzer, `-DLJ_FUZZ=ON`, clang).
- Trade mark: Art-Net is a trade mark of Artistic Licence.
