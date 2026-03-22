#!/usr/bin/env python3
"""
install.py — Instala apps no disco DiskFS do StellaresOS
Uso: python3 install.py stellares.img hello sysinfo

Layout DiskFS:
  Setor 0:    Superbloco  (magic=STEL, nfiles)
  Setores 1-4: Inodes     (32 entradas x 64 bytes = 8 por setor)
  Setores 5+:  Dados      (8 setores = 4096 bytes por arquivo)
"""
import sys, struct, os

SECTOR   = 512
MAGIC    = 0x4B505453  # "STPK" — mas diskfs usa "STEL"
DISKMAGIC= 0x5354454C  # DISKFS_MAGIC do diskfs.h
INODE_SECTORS = 4
DATA_START    = 5
FILE_SECTORS  = 8
FILE_SIZE_MAX = FILE_SECTORS * SECTOR  # 4096 bytes
MAX_FILES     = 32
INODE_SIZE    = 64  # bytes por inode

def read_sector(f, n):
    f.seek(n * SECTOR)
    return f.read(SECTOR)

def write_sector(f, n, data):
    assert len(data) <= SECTOR
    data = data + b'\x00' * (SECTOR - len(data))
    f.seek(n * SECTOR)
    f.write(data)

def read_super(f):
    data = read_sector(f, 0)
    magic, version, nfiles = struct.unpack_from('<III', data, 0)
    label = data[20:36].rstrip(b'\x00').decode('ascii', errors='replace')
    return magic, nfiles, label

def read_inodes(f):
    inodes = []
    raw = b''
    for s in range(1, 1+INODE_SECTORS):
        raw += read_sector(f, s)
    # Each inode: name(48) + size(4) + slot(4) + used(1) + is_dir(1) + pad(6) = 64 bytes
    for i in range(MAX_FILES):
        off = i * INODE_SIZE
        chunk = raw[off:off+INODE_SIZE]
        name  = chunk[0:48].rstrip(b'\x00').decode('ascii', errors='replace')
        size  = struct.unpack_from('<I', chunk, 48)[0]
        slot  = struct.unpack_from('<I', chunk, 52)[0]
        used  = chunk[56]
        inodes.append({'name': name, 'size': size, 'slot': slot, 'used': used})
    return inodes

def write_inodes(f, inodes):
    raw = b''
    for inode in inodes:
        name_b = inode['name'].encode('ascii')[:47].ljust(48, b'\x00')
        chunk  = name_b
        chunk += struct.pack('<I', inode['size'])
        chunk += struct.pack('<I', inode['slot'])
        chunk += bytes([1 if inode['used'] else 0])  # used
        chunk += bytes([0])  # is_dir
        chunk += bytes(6)    # pad
        raw += chunk
    # Escreve nos 4 setores de inodes
    for s in range(INODE_SECTORS):
        write_sector(f, 1+s, raw[s*SECTOR:(s+1)*SECTOR])

def write_super(f, nfiles):
    data = read_sector(f, 0)
    data = bytearray(data)
    struct.pack_into('<I', data, 8, nfiles)  # nfiles está no offset 8
    write_sector(f, 0, bytes(data))

def find_free_slot(inodes):
    used_slots = {i['slot'] for i in inodes if i['used']}
    for s in range(MAX_FILES):
        if s not in used_slots:
            return s
    return -1

def find_free_inode(inodes):
    for i, inode in enumerate(inodes):
        if not inode['used']:
            return i
    return -1

def install_app(disk_path, app_path):
    app_name = os.path.basename(app_path)

    with open(app_path, 'rb') as af:
        app_data = af.read()

    if len(app_data) > FILE_SIZE_MAX:
        print(f"  ERRO: {app_name} muito grande ({len(app_data)} > {FILE_SIZE_MAX})")
        return False

    with open(disk_path, 'r+b') as f:
        magic, nfiles, label = read_super(f)

        if magic != DISKMAGIC:
            print(f"  AVISO: Disco nao formatado (magic=0x{magic:08X})")
            print(f"  Execute make run-disk primeiro para formatar o disco.")
            return False

        inodes = read_inodes(f)

        # Verifica se já existe
        existing = next((i for i,n in enumerate(inodes)
                        if n['used'] and n['name']==app_name), -1)

        if existing >= 0:
            # Atualiza existente
            slot = inodes[existing]['slot']
            print(f"  Atualizando: {app_name} -> slot {slot}")
        else:
            # Aloca novo
            idx  = find_free_inode(inodes)
            slot = find_free_slot(inodes)
            if idx < 0 or slot < 0:
                print(f"  ERRO: Disco cheio!")
                return False
            inodes[idx] = {'name': app_name, 'size': len(app_data),
                          'slot': slot, 'used': 1}
            nfiles += 1
            print(f"  Instalando: {app_name} ({len(app_data)} bytes) -> slot {slot}")

        # Escreve dados (padded para 4KB)
        padded = app_data + b'\x00' * (FILE_SIZE_MAX - len(app_data))
        lba = DATA_START + slot * FILE_SECTORS
        for s in range(FILE_SECTORS):
            write_sector(f, lba+s, padded[s*SECTOR:(s+1)*SECTOR])

        # Atualiza inodes e superbloco
        write_inodes(f, inodes)
        write_super(f, nfiles)

    print(f"  OK: {app_name} instalado!")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Uso: python3 install.py <disco.img> <app1> [app2] ...")
        sys.exit(1)

    disk = sys.argv[1]
    apps = sys.argv[2:]

    print(f"\n  Disco: {disk}")
    print(f"  Apps:  {', '.join(apps)}")
    print()

    ok = 0
    for app in apps:
        if os.path.exists(app):
            if install_app(disk, app):
                ok += 1
        else:
            print(f"  AVISO: {app} nao encontrado, pule 'make apps' primeiro")

    print(f"\n  {ok}/{len(apps)} apps instalados com sucesso.\n")
