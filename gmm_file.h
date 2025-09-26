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
#ifndef GMMFILE_H
#define GMMFILE_H

#include <stddef.h>
#include <stdio.h>

#include "defs.h"
#include "dynarray.h"

typedef struct Context {
  char *file_name;
} Context;

typedef struct RiffFile {
  uint16 length;
  uint8 *data;
} RiffFile;

typedef struct RiffChunkHeader {
  uint8 ckId[4];
  uint32 ckSize;
} RiffChunkHeader;

typedef struct RiffChunkList {
  RiffChunkHeader head;
  uint8 ckType[4];
  // children is an array with elements of type GmmChunk
  Dynarray children;
} RiffChunkList;

typedef struct RiffChunkUnknown {
  RiffChunkHeader head;
} RiffChunkUnknown;

typedef struct RiffChunkMapProperties {
  RiffChunkHeader head;
  uint16 version;
  char *title;         // mallocd, freed in free_chunks
  char *game;          // mallocd, freed in free_chunks
  char *author;        // mallocd, freed in free_chunks
  char *creation_time; // mallocd, freed in free_chunks
  char *notes;         // mallocd, freed in free_chunks
} RiffChunkMapProperties;

typedef struct RiffChunkMapCoords {
  RiffChunkHeader head;
  uint8 origin;
  uint8 row_style;
  uint8 column_style;
  uint16 row_start;
  uint16 column_start;
} RiffChunkMapCoords;

typedef struct RiffChunkLevelProperties {
  RiffChunkHeader head;
  char *location_name;
  char *level_name;
  int16 elevation;
  uint16 num_rows;
  uint16 num_columns;
  uint8 override_coord_opts;
  char *notes;
} RiffChunkLevelProperties;

typedef struct RiffChunkLevelCoords {
  RiffChunkHeader head;
  uint8 origin;
  uint8 row_style;
  uint8 column_style;
  uint16 row_start;
  uint16 column_start;
} RiffChunkLevelCoords;

typedef struct RiffChunkLevelCell {
  RiffChunkHeader head;
  uint8 *floor;
  uint8 *floor_orientation;
  uint8 *floor_color;
  uint8 *wall_north;
  uint8 *wall_west;
  uint8 *trail;
  size_t cells_count;
} RiffChunkLevelCell;

typedef struct IndexedAnnotation {
  uint16 index;
  uint8 index_color;
} IndexedAnnotation;

typedef struct CustomIdAnnotation {
  char *custom_id;
} CustomIdAnnotation;

typedef struct IconAnnotation {
  uint8 icon;
} IconAnnotation;

typedef struct LabelAnnotation {
  uint8 label_color;
} LabelAnnotation;

typedef enum AnnotationKind {
  AK_COMMENT = 0,
  AK_INDEXED,
  AK_CUSTOM,
  AK_ICON,
  AK_LABEL,
} AnnotationKind;

typedef struct AnnotationRecord {
  uint16 row;
  uint16 column;
  AnnotationKind kind;
  char *text;

  union {
    IndexedAnnotation indexed;
    CustomIdAnnotation custom;
    IconAnnotation icon;
    LabelAnnotation label;
  };
} AnnotationRecord;

typedef struct RiffChunkLevelAnno {
  RiffChunkHeader head;
  uint16 num_annotations;
  AnnotationRecord *records;
} RiffChunkLevelAnno;

typedef struct LevelRegionRecord {
  char *name;
  char *notes;
} LevelRegionRecord;

typedef struct RiffChunkLevelRegn {
  RiffChunkHeader head;
  uint8 enable_regions;
  uint16 rows_per_region;
  uint16 columns_per_region;
  uint16 num_regions;
  uint8 per_region_coords;
  LevelRegionRecord *records;
} RiffChunkLevelRegn;

typedef struct MapLinksRecord {
  uint16 src_level_index;
  uint16 src_row;
  uint16 src_column;
  uint16 dest_level_index;
  uint16 dest_row;
  uint16 dest_column;
} MapLinksRecord;

typedef struct RiffChunkMapLinks {
  RiffChunkHeader head;
  uint16 num_links;
  MapLinksRecord *records;
} RiffChunkMapLinks;

static char *chunk_names[] = {
    "LIST",     "MAP_PROP", "MAP_COOR", "LVL_PROP",  "LVL_COOR",
    "LVL_CELL", "LVL_ANNO", "LVL_REGN", "MAP_LINKS",
};

typedef enum GmmChunkType {
  GMM_LIST = 0,
  GMM_MAP_PROP,
  GMM_MAP_COOR,
  GMM_LVL_PROP,
  GMM_LVL_COOR,
  GMM_LVL_CELL,
  GMM_LVL_ANNO,
  GMM_LVL_REGN,
  GMM_MAP_LINKS,
  GMM_UNKNOWN = 255,
} GmmChunkType;

typedef struct GmmChunk {
  union {
    RiffChunkList list_chunk;
    RiffChunkUnknown unknown_chunk;
    RiffChunkMapProperties map_prop_chunk;
    RiffChunkMapCoords map_coor_chunk;
    RiffChunkLevelProperties level_prop_chunk;
    RiffChunkLevelCoords level_coor_chunk;
    RiffChunkLevelCell level_cell_chunk;
    RiffChunkLevelAnno level_anno_chunk;
    RiffChunkLevelRegn level_regn_chunk;
    RiffChunkMapLinks map_links_chunk;
  };
  GmmChunkType ctype;
} GmmChunk;

struct DecodingCursor;
struct DecodingContext;

void free_gmmfile(RiffFile *);
Dynarray decode_chunks(RiffFile *);
void free_chunks(Dynarray *chunk_array);
char *chunk_type_to_str(GmmChunkType ck_type);

RiffFile read_riff(FILE *fstr, const Context *ctx);

#endif // GMMFILE_H
