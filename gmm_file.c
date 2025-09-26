/*
    gmm2json: program that reads Gridmonger's GMM file and converts it into
   JSON format
    Copyright (C) 2025 Jagholin (github.com/Jagholin)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see
   <https://www.gnu.org/licenses/>
*/
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "gmm_file.h"

struct DecodingCursor {
  const uint8 **data;
  size_t *len;
  struct DecodingCursor *parent;
};

struct DecodingContext {
  size_t level_size;
  const char *list_type;
};

RESULT
advance_cursor(struct DecodingCursor cursor, size_t delta) {
  if (*cursor.len < delta)
    return RES_ERR;
  *cursor.data += delta;
  *cursor.len -= delta;
  struct DecodingCursor *par = cursor.parent;
  while (par != NULL) {
    *par->len -= delta;
    par = par->parent;
  }
  return RES_OK;
}

struct DecodingCursor recursive_cursor_from(struct DecodingCursor *cursor,
                                            size_t *new_len) {
  if (*new_len > *cursor->len) {
    last_error = RES_BUFFER_TOO_SMALL;
    struct DecodingCursor null_cursor = {NULL, NULL, NULL};
    return null_cursor;
  }
  struct DecodingCursor result = {cursor->data, new_len, cursor};
  return result;
}

void free_gmmfile(RiffFile *f) { free(f->data); }

char *decode_wstr(struct DecodingCursor cursor) {
  char *result = NULL;
  // We have a size prefix in front
  uint16 *str_len = (uint16 *)*cursor.data;
  advance_cursor(cursor, 2);
  PROPAGATEERR();

  if (*str_len > *cursor.len) {
    // size prefix indicates size that is more than the data stream
    last_error = RES_BUFFER_TOO_SMALL;
    goto onpropagate;
  }

  result = malloc(*str_len + 1);
  memset(result, 0, *str_len + 1);
  OOMERROR(result);
  strncpy(result, (char *)*cursor.data, *str_len);
  advance_cursor(cursor, *str_len);
  PROPAGATEERR();
  return result;
onpropagate:
  if (result)
    free(result);
  return NULL;
onoom:
  exit(EXIT_FAILURE);
}

char *decode_bstr(struct DecodingCursor cursor) {
  // We have a size prefix in front
  char *result = NULL;
  uint8 *str_len = (uint8 *)*cursor.data;
  advance_cursor(cursor, 1);
  PROPAGATEERR();

  if (*str_len > *cursor.len) {
    // size prefix indicates size that is more than the data stream
    last_error = RES_BUFFER_TOO_SMALL;
    return NULL;
  }

  result = malloc(*str_len + 1);
  memset(result, 0, *str_len + 1);
  OOMERROR(result);
  strncpy(result, (char *)*cursor.data, *str_len);
  advance_cursor(cursor, *str_len);
  PROPAGATEERR();
  return result;
onpropagate:
  if (result)
    free(result);
  return NULL;
onoom:
  exit(EXIT_FAILURE);
}

uint8 *decode_cell_layer(struct DecodingCursor cursor, size_t size) {
  // See if we have compression
  uint8 *result = NULL;
  const uint8 *compression_type = *cursor.data;
  advance_cursor(cursor, 1);
  PROPAGATEERR();
  result = malloc(size * sizeof(uint8));
  OOMERROR(result);
  if (*compression_type == 0) {
    // No compression, just memcpy.
    const uint8 *src_data = *cursor.data;
    advance_cursor(cursor, size);
    PROPAGATEERR();
    memcpy(result, src_data, size);
  } else if (*compression_type == 1) {
    const uint32 *compressed_length = (const uint32 *)*cursor.data;
    advance_cursor(cursor, sizeof(uint32));
    PROPAGATEERR();

    memset(result, 0, size);
    const uint8 *compressed_data = *cursor.data;
    const uint8 *compressed_end = compressed_data + *compressed_length;
    advance_cursor(cursor, *compressed_length);
    PROPAGATEERR();

    uint8 *dest = result;
    while (compressed_data < compressed_end) {
      uint8 next_byte = *compressed_data;
      if (next_byte & 0x80) {
        compressed_data += 1;
        uint8 repeat_len = (next_byte & (0x7f)) + 1;

        // buffer overflow check
        CHECKERR(dest + repeat_len > result + size,
                 "Possible buffer overflow in decode_cell_layer, line %d. "
                 "Aborting...\n",
                 __LINE__);
        CHECKERR(compressed_data == compressed_end,
                 "compressed_data unexpectedly run out on line %d\n", __LINE__);
        memset(dest, *compressed_data, repeat_len);

        dest += repeat_len;
        compressed_data += 1;
      } else {
        CHECKERR(dest + 1 > result + size,
                 "Possible buffer overflow in decode_cell_layer, line %d. "
                 "Aborting...\n",
                 __LINE__);
        CHECKERR(compressed_data == compressed_end,
                 "compressed_data unexpectedly run out on line %d\n", __LINE__);
        *dest = *compressed_data;
        dest += 1;
        compressed_data += 1;
      }
    }
  } else if (*compression_type == 2) {
    memset(result, 0, size);
  } else {
    // Unexpected value of compression_type
    last_error = RES_BAD_INPUT;
    goto onerror;
  }

  return result;

onpropagate:
onerror:
  if (result)
    free(result);
  return NULL;
onoom:
  exit(EXIT_FAILURE);
}

size_t decode_map_prop_chunk(struct DecodingCursor cursor,
                             RiffChunkMapProperties *out) {
  size_t start_len = *cursor.len;
  out->version = *(uint16 *)*cursor.data;
  advance_cursor(cursor, 2);
  PROPAGATEERR();
  CHECKERR((out->title = decode_wstr(cursor)) == NULL,
           "Error decoding WSTR map_prop.title");
  CHECKERR((out->game = decode_wstr(cursor)) == NULL,
           "Error decoding WSTR map_prop.game");
  CHECKERR((out->author = decode_wstr(cursor)) == NULL,
           "Error decoding WSTR map_prop.author");
  CHECKERR((out->creation_time = decode_bstr(cursor)) == NULL,
           "Error decoding BSTR map_prop.creation_time");
  CHECKERR((out->notes = decode_wstr(cursor)) == NULL,
           "Error decoding WSTR map_prop.notes");
  return start_len - *cursor.len;
onpropagate:
onerror:
  exit(EXIT_FAILURE);
}

size_t decode_map_coor_chunk(struct DecodingCursor cursor,
                             RiffChunkMapCoords *out) {
  PACKED_STRUCT DecodedData {
    uint8 origin;
    uint8 row_style;
    uint8 column_style;
    uint16 row_start;
    uint16 column_start;
  }
  *decoded_data = (struct DecodedData *)*cursor.data;
  advance_cursor(cursor, sizeof(struct DecodedData));
  PROPAGATEERR();
  out->origin = decoded_data->origin;
  out->row_style = decoded_data->row_style;
  out->column_style = decoded_data->column_style;
  out->row_start = decoded_data->row_start;
  out->column_start = decoded_data->column_start;
  return sizeof(struct DecodedData);
onpropagate:
  exit(EXIT_FAILURE);
}

size_t decode_lvl_prop_chunk(struct DecodingCursor cursor,
                             RiffChunkLevelProperties *out) {
  size_t start_len = *cursor.len;
  out->location_name = decode_wstr(cursor);
  PROPAGATEERR();
  out->level_name = decode_wstr(cursor);
  PROPAGATEERR();
  PACKED_STRUCT DecodedData {
    int16 elevation;
    uint16 num_rows;
    uint16 num_columns;
    uint8 override_coord_opts;
  }
  *decoded_data = (struct DecodedData *)*cursor.data;
  advance_cursor(cursor, sizeof(struct DecodedData));
  PROPAGATEERR();
  out->elevation = decoded_data->elevation;
  out->num_rows = decoded_data->num_rows;
  out->num_columns = decoded_data->num_columns;
  out->override_coord_opts = decoded_data->override_coord_opts;
  out->notes = decode_wstr(cursor);
  PROPAGATEERR();
  return start_len - *cursor.len;
onpropagate:
  exit(EXIT_FAILURE);
}

size_t decode_lvl_coor_chunk(struct DecodingCursor cursor,
                             RiffChunkLevelCoords *out) {
  PACKED_STRUCT DecodedData {
    uint8 origin;
    uint8 row_style;
    uint8 column_style;
    uint16 row_start;
    uint16 column_start;
  }
  *decoded_data = (struct DecodedData *)*cursor.data;
  advance_cursor(cursor, sizeof(struct DecodedData));
  PROPAGATEERR();
  out->origin = decoded_data->origin;
  out->row_style = decoded_data->row_style;
  out->column_style = decoded_data->column_style;
  out->row_start = decoded_data->row_start;
  out->column_start = decoded_data->column_start;
  return sizeof(struct DecodedData);
onpropagate:
  exit(EXIT_FAILURE);
}

size_t decode_lvl_cell_chunk(struct DecodingCursor cursor,
                             RiffChunkLevelCell *out, size_t cell_count) {
  const uint8 *start_addr = *cursor.data;
  out->floor = decode_cell_layer(cursor, cell_count);
  PROPAGATEERR();
  out->floor_orientation = decode_cell_layer(cursor, cell_count);
  PROPAGATEERR();
  out->floor_color = decode_cell_layer(cursor, cell_count);
  PROPAGATEERR();
  out->wall_north = decode_cell_layer(cursor, cell_count);
  PROPAGATEERR();
  out->wall_west = decode_cell_layer(cursor, cell_count);
  PROPAGATEERR();
  out->trail = decode_cell_layer(cursor, cell_count);
  PROPAGATEERR();
  out->cells_count = cell_count;

  return *cursor.data - start_addr;

onpropagate:
  exit(EXIT_FAILURE);
}

size_t decode_lvl_anno_chunk(struct DecodingCursor cursor,
                             RiffChunkLevelAnno *out) {
  const uint8 *start_addr = *cursor.data;
  const uint16 *num_annos = (const uint16 *)*cursor.data;
  advance_cursor(cursor, sizeof(uint16));
  PROPAGATEERR();

  out->num_annotations = *num_annos;
  out->records = malloc(sizeof(AnnotationRecord) * (*num_annos));
  OOMERROR(out->records);

  for (uint16 i = 0; i < *num_annos; ++i) {
    const PACKED_STRUCT DecodedData {
      uint16 row;
      uint16 column;
      uint8 kind;
    }
    *decoded_data = (const struct DecodedData *)*cursor.data;
    advance_cursor(cursor, sizeof(struct DecodedData));
    PROPAGATEERR();
    out->records[i].row = decoded_data->row;
    out->records[i].column = decoded_data->column;
    out->records[i].kind = decoded_data->kind;

    if (decoded_data->kind == AK_INDEXED) {
      // Indexed Annotation
      out->records[i].indexed.index = *(const uint16 *)*cursor.data;
      advance_cursor(cursor, 2);
      PROPAGATEERR();
      out->records[i].indexed.index_color = **cursor.data;
      advance_cursor(cursor, 1);
      PROPAGATEERR();
    } else if (decoded_data->kind == AK_CUSTOM) {
      // custom id annotation
      out->records[i].custom.custom_id = decode_bstr(cursor);
      PROPAGATEERR();
    } else if (decoded_data->kind == AK_ICON) {
      // icon annotation
      out->records[i].icon.icon = **cursor.data;
      advance_cursor(cursor, 1);
      PROPAGATEERR();
    } else if (decoded_data->kind == AK_LABEL) {
      // label annotation
      out->records[i].label.label_color = **cursor.data;
      advance_cursor(cursor, 1);
      PROPAGATEERR();
    }

    out->records[i].text = decode_wstr(cursor);
    PROPAGATEERR();
  }

  return *cursor.data - start_addr;

onpropagate:
onoom:
  exit(EXIT_FAILURE);
}

size_t decode_lvl_regn_chunk(struct DecodingCursor cursor,
                             RiffChunkLevelRegn *out) {
  const uint8 *start_addr = *cursor.data;
  const PACKED_STRUCT DecodedData {
    uint8 enable_regions;
    uint16 row_per_region;
    uint16 columns_per_region;
    uint8 per_region_coords;
    uint16 num_regions;
  }
  *decoded_data = (const struct DecodedData *)*cursor.data;
  advance_cursor(cursor, sizeof(struct DecodedData));
  PROPAGATEERR();

  out->enable_regions = decoded_data->enable_regions;
  out->rows_per_region = decoded_data->row_per_region;
  out->columns_per_region = decoded_data->columns_per_region;
  out->per_region_coords = decoded_data->per_region_coords;
  out->num_regions = decoded_data->num_regions;
  out->records = malloc(sizeof(LevelRegionRecord) * out->num_regions);
  OOMERROR(out->records);

  // printf("Decoding regions: %u regions total\n", out->num_regions);

  for (uint16 i = 0; i < decoded_data->num_regions; ++i) {
    out->records[i].name = decode_wstr(cursor);
    PROPAGATEERR();
    out->records[i].notes = decode_wstr(cursor);
    PROPAGATEERR();
    // printf("Decoded region %u with name: '%s' with notes: '%s'\n", i,
    //       out->records[i].name, out->records[i].notes);
  }

  return *cursor.data - start_addr;
onpropagate:
onoom:
  exit(EXIT_FAILURE);
}

size_t decode_map_links_chunk(const struct DecodingCursor cursor,
                              RiffChunkMapLinks *out) {
  const uint8 *start_addr = *cursor.data;
  const uint16 *num_links = (const uint16 *)*cursor.data;
  advance_cursor(cursor, 2);
  PROPAGATEERR();
  out->num_links = *num_links;
  out->records = malloc(sizeof(MapLinksRecord) * (*num_links));
  OOMERROR(out->records);

  for (uint16 i = 0; i < *num_links; ++i) {

    const PACKED_STRUCT DecodedData {
      uint16 src_level_index;
      uint16 src_row;
      uint16 src_column;
      uint16 dest_level_index;
      uint16 dest_row;
      uint16 dest_column;
    }
    *decoded_data = (const struct DecodedData *)*cursor.data;
    advance_cursor(cursor, sizeof(struct DecodedData));
    PROPAGATEERR();

    out->records[i].src_level_index = decoded_data->src_level_index;
    out->records[i].src_row = decoded_data->src_row;
    out->records[i].src_column = decoded_data->src_column;
    out->records[i].dest_level_index = decoded_data->dest_level_index;
    out->records[i].dest_row = decoded_data->dest_row;
    out->records[i].dest_column = decoded_data->dest_column;
  }

  return *cursor.data - start_addr;
onpropagate:
onoom:
  exit(EXIT_FAILURE);
}

// Decodes GMM RIFF chunks while advancing the data pointer
// Arguments:
//    data (in/out) -> *data points to the data that needs to be decoded.
//                     after return, *data will point to the undecoded tail.
//    len (in/out)  -> *len is the length of the **data array.
//                     after return, *len will have the length of undecoded
//                     tail.
//    out (out)     -> *out is a dynarray of GmmChunk s. You have to initialize
//    one with make_dynarray,
//                     then pass it to this function.
//    ctx (in)      -> context that is needed to decode some of the chunks.
//    Internal var, if you call
//                     it yourself, just pass NULL.
//
// Returns size_t length of decoded part of the *data array.
size_t _decode_chunks(struct DecodingCursor dc, Dynarray *out,
                      struct DecodingContext *ctx) {
  const char *ignore_list[] = {"disp", "opts", "tool", "notl", "\0"};

  size_t decoded_length = 0;
  while (*dc.len > 0) {
    last_error = 0;
    CHECKERR(*dc.len < 4,
             "Unexpected end of a chunk. The file might be damaged.\n");
    PACKED_STRUCT HeaderPacked {
      char ckId[4];
      uint32 ckSize;
    }
    *header = (struct HeaderPacked *)*dc.data;
    decoded_length += sizeof(struct HeaderPacked);
    advance_cursor(dc, sizeof(struct HeaderPacked));
    CHECKRESULT("Unexpectedly run out of bytes while decoding");
    size_t size_check = *dc.len;

    GmmChunk *new_chunk = dynarray_push_inplace(out);
    RiffChunkHeader *new_header = (RiffChunkHeader *)new_chunk;
    strncpy((char *)new_header->ckId, header->ckId, 4);
    new_header->ckSize = header->ckSize;
    new_chunk->ctype = GMM_UNKNOWN;

    // If the chunk type is on the ignore_list, we just skip it
    bool ignore_this = false;
    for (size_t i = 0; ignore_list[i][0] != '\0'; i++) {
      if (strncmp(header->ckId, ignore_list[i], 4) == 0) {
        ignore_this = true;
        break;
      }
    }

    if (ignore_this) {
      // Advance by header.ckSize;
      advance_cursor(dc, header->ckSize);
      PROPAGATEERR();
    } else if (strncmp(header->ckId, "LIST", 4) == 0) {
      new_chunk->ctype = GMM_LIST;
      // the list type is the next 4 bytes after the header
      CHECKERR(*dc.len < 4,
               "Unexpected end of a chunk. The file might be damaged.\n");
      strncpy((char *)new_chunk->list_chunk.ckType, (const char *)*dc.data, 4);
      decoded_length += 4;
      advance_cursor(dc, 4);
      CHECKRESULT("Unexpectedly run out of bytes while decoding");
      new_chunk->list_chunk.children = make_dynarray(sizeof(GmmChunk), 1);
      size_t list_len = header->ckSize - 4;
      struct DecodingCursor nested_cursor =
          recursive_cursor_from(&dc, &list_len);
      PROPAGATEERR();
      struct DecodingContext new_ctx;
      memcpy(&new_ctx, ctx, sizeof(struct DecodingContext));
      new_ctx.list_type = (const char *)new_chunk->list_chunk.ckType;
      _decode_chunks(nested_cursor, &new_chunk->list_chunk.children, &new_ctx);
      // advance len by the amount of bytes that were just read
      // *dc.len -= header->ckSize - 4 - list_len;
    } else if (strncmp(header->ckId, "prop", 4) == 0) {
      // this is either map prop chunk or lvl prop chunk depending on context
      if (strncmp(ctx->list_type, "map ", 4) == 0) {
        new_chunk->ctype = GMM_MAP_PROP;
        decoded_length += decode_map_prop_chunk(dc, &new_chunk->map_prop_chunk);
      } else if (strncmp(ctx->list_type, "lvl ", 4) == 0) {
        new_chunk->ctype = GMM_LVL_PROP;
        decoded_length +=
            decode_lvl_prop_chunk(dc, &new_chunk->level_prop_chunk);
        size_t level_size = (new_chunk->level_prop_chunk.num_columns + 1) *
                            (new_chunk->level_prop_chunk.num_rows + 1);
        ctx->level_size = level_size;
      }
    } else if (strncmp(header->ckId, "coor", 4) == 0) {
      if (strncmp(ctx->list_type, "map ", 4) == 0) {
        new_chunk->ctype = GMM_MAP_COOR;
        decoded_length += decode_map_coor_chunk(dc, &new_chunk->map_coor_chunk);
      } else if (strncmp(ctx->list_type, "lvl ", 4) == 0) {
        new_chunk->ctype = GMM_LVL_COOR;
        decoded_length +=
            decode_lvl_coor_chunk(dc, &new_chunk->level_coor_chunk);
      }
    } else if (strncmp(header->ckId, "cell", 4) == 0) {
      new_chunk->ctype = GMM_LVL_CELL;
      decoded_length += decode_lvl_cell_chunk(dc, &new_chunk->level_cell_chunk,
                                              ctx->level_size);
    } else if (strncmp(header->ckId, "anno", 4) == 0) {
      new_chunk->ctype = GMM_LVL_ANNO;
      decoded_length += decode_lvl_anno_chunk(dc, &new_chunk->level_anno_chunk);
    } else if (strncmp(header->ckId, "lnks", 4) == 0) {
      new_chunk->ctype = GMM_MAP_LINKS;
      decoded_length += decode_map_links_chunk(dc, &new_chunk->map_links_chunk);
    } else if (strncmp(header->ckId, "regn", 4) == 0) {
      new_chunk->ctype = GMM_LVL_REGN;
      decoded_length += decode_lvl_regn_chunk(dc, &new_chunk->level_regn_chunk);
    }
    if (!ignore_this && size_check - *dc.len != header->ckSize) {
      long int size_defect = header->ckSize - (size_check - *dc.len);
      // REally shouldn't happen, we decoded more bytes than the buffer length.
      CHECKERR(header->ckSize < size_check - *dc.len,
               "Suspected buffer overrun in _decode_chunks of size %ld. "
               "Aborting...\n",
               size_defect);
      // printf("%ld bytes remain undecoded in chunk %.4s. Skipping...\n",
      //       size_defect, header->ckId);

      advance_cursor(dc, size_defect);
      decoded_length += size_defect;
    }
    // Chunks are word aligned, so we need to skip 1 byte if necessary
    if (header->ckSize % 2 == 1) {
      advance_cursor(dc, 1);
      decoded_length += 1;
    }
  }
  return decoded_length;
onpropagate:
onerror:
  exit(EXIT_FAILURE);
}

Dynarray decode_chunks(RiffFile *file) {
  Dynarray result = make_dynarray(sizeof(GmmChunk), 2);
  const uint8 *file_data = file->data;
  size_t data_size = file->length;
  struct DecodingCursor cursor = {&file_data, &data_size, NULL};
  struct DecodingContext ctx = {0, NULL};
  _decode_chunks(cursor, &result, &ctx);
  return result;
}

void free_chunks(Dynarray *chunk_array) {
  for (unsigned int i = 0; i < dynarray_size(chunk_array); ++i) {
    GmmChunk *ck = (GmmChunk *)dynarray_get(chunk_array, i);
    switch (ck->ctype) {
    case GMM_LIST:
      free_chunks(&ck->list_chunk.children);
      break;
    case GMM_MAP_PROP:
      free(ck->map_prop_chunk.author);
      free(ck->map_prop_chunk.creation_time);
      free(ck->map_prop_chunk.game);
      free(ck->map_prop_chunk.notes);
      free(ck->map_prop_chunk.title);
      break;
    default:
      break;
    }
  }
  dynarray_free(chunk_array);
}

RiffFile read_riff(FILE *fstr, const Context *ctx) {
  PACKED_STRUCT {
    uint8 ckId[4];
    uint32 ckSize;
    uint8 formType[4];
  }
  header;
  // read the RIFF header of GMM file
  RiffFile result = {0, NULL};
  size_t readlen = fread(&header, sizeof(header), 1, fstr);
  uint32 remainder_len;
  uint8 *remainder_bytes = NULL;

  CHECKERR(readlen == 0, "Couldn't read data from file: %s", ctx->file_name);
  // Check for correct bytes
  CHECKERR(strncmp((const char *)header.ckId, "RIFF", 4),
           "The file %s is not a RIFF file", ctx->file_name);
  CHECKERR(strncmp((const char *)header.formType, "GRMM", 4),
           "The file %s is not a valid GMM file", ctx->file_name);
  // subtract 4, because we already read 4 bytes of the data chunk
  // add a byte to fulfill alignment requirement.
  remainder_len = header.ckSize - 4 + header.ckSize % 2;
  remainder_bytes = malloc(remainder_len);
  OOMERROR(remainder_bytes);
  readlen = fread(remainder_bytes, 1, remainder_len, fstr);
  if (readlen != remainder_len) {
    printf("Expected to read %u bytes, read only %u bytes.\n", remainder_len,
           (unsigned int)readlen);
    goto onerror;
  }

  result.length = remainder_len;
  result.data = remainder_bytes;
  return result;
onerror:
  if (remainder_bytes)
    free(remainder_bytes);
onoom:
  exit(EXIT_FAILURE);
}

char *chunk_type_to_str(GmmChunkType ck_type) {
  static char *unknown_type = "TYPE_UNKNOWN";
  if (ck_type < sizeof(chunk_names) / sizeof(char *)) {
    return chunk_names[ck_type];
  }
  return unknown_type;
}
