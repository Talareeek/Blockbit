"use strict";

const TagType = Object.freeze({
    T_END: 0,
    T_BOOL: 1,
    T_INT8: 2,
    T_INT16: 3,
    T_INT32: 4,
    T_INT64: 5,
    T_UINT8: 6,
    T_UINT16: 7,
    T_UINT32: 8,
    T_UINT64: 9,
    T_FLOAT: 10,
    T_DOUBLE: 11,
    T_STRING: 12,
    T_LIST: 13,
    T_COMPOUND: 14,
    T_BYTEARRAY: 15,
    T_VECTOR2_FLOAT: 16,
    T_VECTOR2_INT: 17,
    T_VECTOR2_DOUBLE: 18,
});

const TagTypeName = Object.fromEntries(
    Object.entries(TagType).map(([k, v]) => [v, k])
);

const BBT_MAGIC = 2;

class Reader {
    constructor(buffer) {
        this.view = new DataView(buffer);
        this.bytes = new Uint8Array(buffer);
        this.pos = 0;
    }

    get remaining() { return this.view.byteLength - this.pos; }

    needs(n) {
        if (this.pos + n > this.view.byteLength) {
            throw new Error(`Unexpected end of buffer at ${this.pos} (need ${n}, have ${this.remaining})`);
        }
    }

    readBool() { this.needs(1); return this.view.getUint8(this.pos++) !== 0; }
    readInt8() { this.needs(1); const v = this.view.getInt8(this.pos); this.pos += 1; return v; }
    readUInt8() { this.needs(1); return this.view.getUint8(this.pos++); }
    readInt16() { this.needs(2); const v = this.view.getInt16(this.pos, true); this.pos += 2; return v; }
    readUInt16() { this.needs(2); const v = this.view.getUint16(this.pos, true); this.pos += 2; return v; }
    readInt32() { this.needs(4); const v = this.view.getInt32(this.pos, true); this.pos += 4; return v; }
    readUInt32() { this.needs(4); const v = this.view.getUint32(this.pos, true); this.pos += 4; return v; }
    readInt64() { this.needs(8); const v = this.view.getBigInt64(this.pos, true); this.pos += 8; return v; }
    readUInt64() { this.needs(8); const v = this.view.getBigUint64(this.pos, true); this.pos += 8; return v; }
    readFloat() { this.needs(4); const v = this.view.getFloat32(this.pos, true); this.pos += 4; return v; }
    readDouble() { this.needs(8); const v = this.view.getFloat64(this.pos, true); this.pos += 8; return v; }

    readString() {
        const length = this.readUInt16();
        this.needs(length);
        const slice = this.bytes.subarray(this.pos, this.pos + length);
        this.pos += length;
        return new TextDecoder("utf-8", { fatal: false }).decode(slice);
    }

    readByteArray() {
        const length = this.readUInt32();
        this.needs(length);
        const slice = this.bytes.slice(this.pos, this.pos + length);
        this.pos += length;
        return slice;
    }
}

function readTagPayload(type, reader) {
    switch (type) {
        case TagType.T_END: return null;
        case TagType.T_BOOL: return reader.readBool();
        case TagType.T_INT8: return reader.readInt8();
        case TagType.T_INT16: return reader.readInt16();
        case TagType.T_INT32: return reader.readInt32();
        case TagType.T_INT64: return reader.readInt64();
        case TagType.T_UINT8: return reader.readUInt8();
        case TagType.T_UINT16: return reader.readUInt16();
        case TagType.T_UINT32: return reader.readUInt32();
        case TagType.T_UINT64: return reader.readUInt64();
        case TagType.T_FLOAT: return reader.readFloat();
        case TagType.T_DOUBLE: return reader.readDouble();
        case TagType.T_STRING: return reader.readString();
        case TagType.T_BYTEARRAY: return reader.readByteArray();
        case TagType.T_VECTOR2_FLOAT: return { x: reader.readFloat(), y: reader.readFloat() };
        case TagType.T_VECTOR2_INT: return { x: reader.readInt32(), y: reader.readInt32() };
        case TagType.T_VECTOR2_DOUBLE: return { x: reader.readDouble(), y: reader.readDouble() };

        case TagType.T_LIST: {
            const elementType = reader.readUInt8();
            const count = reader.readUInt32();
            const list = new Array(count);
            for (let i = 0; i < count; i++) {
                list[i] = { type: elementType, value: readTagPayload(elementType, reader) };
            }
            return { elementType, items: list };
        }

        case TagType.T_COMPOUND: {
            const entries = [];
            while (true) {
                const childType = reader.readUInt8();
                if (childType === TagType.T_END) break;
                const childName = reader.readString();
                const childValue = readTagPayload(childType, reader);
                entries.push({ name: childName, type: childType, value: childValue });
            }
            return entries;
        }

        default:
            throw new Error(`Unknown TagType ${type} at position ${reader.pos - 1}`);
    }
}

function parseBBT(buffer) {
    const reader = new Reader(buffer);

    const magic = reader.readInt32();
    if (magic !== BBT_MAGIC) {
        throw new Error(`Invalid BBT magic number: ${magic} (expected ${BBT_MAGIC})`);
    }

    const bbtName = reader.readString();

    const rootType = reader.readUInt8();
    const rootName = reader.readString();
    const rootValue = readTagPayload(rootType, reader);

    return {
        name: bbtName,
        rootName,
        rootType,
        root: rootValue,
        trailing: reader.remaining,
    };
}

window.BBT = { parseBBT, TagType, TagTypeName };
