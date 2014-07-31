// Monary - Copyright 2011-2014 David J. C. Beach
// Please see the included LICENSE.TXT and NOTICE.TXT for licensing information.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mongoc.h"
#include "bson.h"

#ifndef NDEBUG
#define DEBUG(format, ...) fprintf(stderr, "[DEBUG] %s:%i " format "\n", __FILE__, __LINE__, __VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define MONARY_MAX_NUM_COLUMNS 1024
#define MONARY_MAX_STRING_LENGTH 1024
#define MONARY_MAX_QUERY_LENGTH 4096
#define MONARY_MAX_RECURSION 100

enum {
    TYPE_UNDEFINED = 0,
    TYPE_OBJECTID = 1,
    TYPE_BOOL = 2,
    TYPE_INT8 = 3,
    TYPE_INT16 = 4,
    TYPE_INT32 = 5,
    TYPE_INT64 = 6,
    TYPE_UINT8 = 7,
    TYPE_UINT16 = 8,
    TYPE_UINT32 = 9,
    TYPE_UINT64 = 10,
    TYPE_FLOAT32 = 11,
    TYPE_FLOAT64 = 12,
    TYPE_DATE = 13,      // BSON date-time, seconds since the UNIX epoch (uint64 storage)
    TYPE_TIMESTAMP = 14, // BSON timestamp - UNIX timestamp and increment (uint64 storage)
    TYPE_STRING = 15,    // each record is (type_arg) chars in length
    TYPE_BINARY = 16,    // each record is (type_arg) bytes in length
    TYPE_BSON = 17,      // BSON subdocument as binary (each record is type_arg bytes)
    TYPE_TYPE = 18,      // BSON type code (uint8 storage)
    TYPE_SIZE = 19,      // data size of a string, symbol, binary, or bson object (uint32)
    TYPE_LENGTH = 20,    // length of string (character count) or num elements in BSON (uint32)
    LAST_TYPE = 20       // BSON type code as per the BSON specificaion
};

/**
 * Controls the logging performed by libmongoc.
 */
void monary_log_func (mongoc_log_level_t log_level,
                      const char* log_domain,
                      const char* message,
                      void* user_data)
{
    return;
}

/**
 * Initialize libmongoc.
 */
void monary_init(void) {
    mongoc_init();
#ifdef NDEBUG
    mongoc_log_set_handler(monary_log_func, NULL);
#endif
    DEBUG("%s", "monary module initialized");
}

/**
 * Releases resources used by libmongoc.
 */
void monary_cleanup(void) {
    mongoc_cleanup();
    DEBUG("%s", "monary module cleaned up");
}

/**
 * Makes a new connection to a MongoDB server and database.
 *
 * @param uri A MongoDB URI, as per mongoc_uri(7).
 *
 * @return A pointer to a mongoc_client_t, or NULL if the connection attempt
 * was unsuccessful.
 */
mongoc_client_t* monary_connect(const char* uri) {
    mongoc_client_t* client;
    if (!uri) {
        return NULL;
    }

    DEBUG("Attempting connection to: %s", uri);
    client = mongoc_client_new(uri);
    if (client) {
        DEBUG("%s", "Connection successful");
    }
    else {
        DEBUG("An error occurred while attempting to connect to %s", uri);
    }
    return client;
}

/**
 * Destroys all resources associated with the client.
 */
void monary_disconnect(mongoc_client_t* client)
{
    DEBUG("%s", "Closing mongoc_client");
    mongoc_client_destroy(client);
}

/**
 * Use a particular database and collection from the given MongoDB client.
 *
 * @param client A mongoc_client_t that has been properly connected to with
 * mongoc_client_new().
 * @param db A valid ASCII C string for the name of the database.
 * @param collection A valid ASCII C string for the collection name.
 *
 * @return If successful, a mongoc_collection_t that can be queried against
 * with mongoc_collection_find(3).
 */
mongoc_collection_t* monary_use_collection(mongoc_client_t* client,
                                           const char* db,
                                           const char* collection)
{
    return mongoc_client_get_collection(client, db, collection);
}

/**
 * Destroys the given collection, allowing you to connect to another one.
 *
 * @param collection The collection to destroy.
 */
void monary_destroy_collection(mongoc_collection_t* collection)
{
    if (collection) {
        DEBUG("%s", "Closing mongoc_collection");
        mongoc_collection_destroy(collection);
    }
}

/**
 * Holds the storage for an array of objects.
 *
 * @memb field The name of the field in the document.
 * @memb type The BSON type identifier, as specified by the Monary type enum.
 * @memb type_arg If type is binary, UTF-8, or document, type_arg specifies the
 * width of the field in bytes.
 * @memb storage A pointer to the location of the "array" in memory. In the
 * Python side of Monary, this points to the start of the NumPy array.
 * @memb mask A pointer to the the "masked array." This is the internal
 * representation of the NumPy ma.array, which corresponds one-to-one to the
 * storage array. A value is masked if and only if an error occurs while
 * loading memory from MongoDB.
 */
typedef struct monary_column_item
{
    char* field;
    unsigned int type;
    unsigned int type_arg;
    void* storage;
    unsigned char* mask;
} monary_column_item;

/**
 * Represents a collection of arrays.
 *
 * @memb num_columns The number of arrays to track, one per field.
 * @memb num_rows The number of elements per array. (Specifically, each
 * monary_column_item.storage contains num_rows elements.)
 * @memb columns A pointer to the first array.
 */
typedef struct monary_column_data
{
    unsigned int num_columns;
    unsigned int num_rows;
    monary_column_item* columns;
} monary_column_data;

/**
 * A MongoDB cursor augmented with Monary column data.
 */
typedef struct monary_cursor {
    mongoc_cursor_t* mcursor;
    monary_column_data* coldata;
} monary_cursor;

/**
 * Allocates heap space for data storage.
 *
 * @param num_columns The number of fields to store (that is, the number of
 * internal monary_column_item structs tracked by the column data structure).
 * Cannot exceed MONARY_MAX_NUM_COLS.
 * @param num_rows The lengths of the arrays managed by each column item.
 *
 * @return A pointer to the newly-allocated column data.
 */
monary_column_data* monary_alloc_column_data(unsigned int num_columns,
                                             unsigned int num_rows)
{
    monary_column_data* result;
    monary_column_item* columns;

    if(num_columns > MONARY_MAX_NUM_COLUMNS) { return NULL; }
    result = (monary_column_data*) malloc(sizeof(monary_column_data));
    columns = (monary_column_item*) calloc(num_columns, sizeof(monary_column_item));

    DEBUG("%s", "Column data allocated");

    result->num_columns = num_columns;
    result->num_rows = num_rows;
    result->columns = columns;

    return result;
}

int monary_free_column_data(monary_column_data* coldata)
{
    int i;
    monary_column_item* col;

    if(coldata == NULL || coldata->columns == NULL) { return 0; }

    for(i = 0; i < coldata->num_columns; i++) {
        col = coldata->columns + i;
        if(col->field != NULL) { free(col->field); }
    }
    free(coldata->columns);
    free(coldata);
    return 1;
}

/**
 * Sets the field value for a particular column and item. This is referenced by
 * Monary._make_column_data.
 *
 * @param coldata A pointer to the column data to modify.
 * @param colnum The number of the column item within the table to modify
 * (representing one data field). Columns are indexed starting from zero.
 * @param field The new name of the column item. Cannot exceed
 * MONARY_MAX_STRING_LENGTH characters in length.
 * @param type The new type of the item.
 * @param type_arg For UTF-8, binary and BSON types, specifies the size of the
 * data.
 * @param storage A pointer to the new location of the data in memory, which
 * cannot be NULL. Note that this does not free(3) the previous storage
 * pointer.
 * @param mask A pointer to the new masked array, which cannot be NULL. It is a
 * programming error to have a masked array different in length from the
 * storage array.
 *
 * @return 1 if the modification was performed successfully; 0 otherwise.
 */
int monary_set_column_item(monary_column_data* coldata,
                           unsigned int colnum,
                           const char* field,
                           unsigned int type,
                           unsigned int type_arg,
                           void* storage,
                           unsigned char* mask)
{
    int len;
    monary_column_item* col;

    if(coldata == NULL) { return 0; }
    if(colnum >= coldata->num_columns) { return 0; }
    if(type == TYPE_UNDEFINED || type > LAST_TYPE) { return 0; }
    if(storage == NULL) { return 0; }
    // if(mask == NULL) { return 0; }
    
    len = strlen(field);
    if(len > MONARY_MAX_STRING_LENGTH) { return 0; }
    
    col = coldata->columns + colnum;

    col->field = malloc(len + 1);
    strcpy(col->field, field);
    
    col->type = type;
    col->type_arg = type_arg;
    col->storage = storage;
    col->mask = mask;

    return 1;
}

int monary_load_objectid_value(const bson_iter_t* bsonit,
                               monary_column_item* citem,
                               int idx)
{
    const bson_oid_t* oid;
    uint8_t* dest;

    if (BSON_ITER_HOLDS_OID(bsonit)) {
        oid = bson_iter_oid(bsonit);
        dest = ((uint8_t*) citem->storage) + (idx * sizeof(bson_oid_t));
        memcpy(dest, oid->bytes, sizeof(bson_oid_t));
        return 1;
    } else {
        return 0;
    }
}

int monary_load_bool_value(const bson_iter_t* bsonit,
                           monary_column_item* citem,
                           int idx)
{
    bool value;

    value = bson_iter_bool(bsonit);
    memcpy(((bool*) citem->storage) + idx, &value, sizeof(bool));
    return 1;
}


#define MONARY_DEFINE_FLOAT_LOADER(FUNCNAME, NUMTYPE)                        \
int FUNCNAME (const bson_iter_t* bsonit,                                     \
              monary_column_item* citem,                                     \
              int idx)                                                       \
{                                                                            \
    NUMTYPE value;                                                           \
    if (BSON_ITER_HOLDS_DOUBLE(bsonit)) {                                    \
        value = (NUMTYPE) bson_iter_double(bsonit);                          \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE));  \
        return 1;                                                            \
    } else if (BSON_ITER_HOLDS_INT32(bsonit)) {                              \
        value = (NUMTYPE) bson_iter_int32(bsonit);                           \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE));  \
        return 1;                                                            \
    } else if (BSON_ITER_HOLDS_INT64(bsonit)) {                              \
        value = (NUMTYPE) bson_iter_int64(bsonit);                           \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE));  \
        return 1;                                                            \
    } else {                                                                 \
        return 0;                                                            \
    }                                                                        \
}

// Floating point
MONARY_DEFINE_FLOAT_LOADER(monary_load_float32_value, float)
MONARY_DEFINE_FLOAT_LOADER(monary_load_float64_value, double)

#define MONARY_DEFINE_INT_LOADER(FUNCNAME, NUMTYPE)                          \
int FUNCNAME (const bson_iter_t* bsonit,                                     \
              monary_column_item* citem,                                     \
              int idx)                                                       \
{                                                                            \
    NUMTYPE value;                                                           \
    if (BSON_ITER_HOLDS_INT32(bsonit)) {                                     \
        value = (NUMTYPE) bson_iter_int32(bsonit);                           \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE));  \
        return 1;                                                            \
    } else if (BSON_ITER_HOLDS_INT64(bsonit)) {                              \
        value = (NUMTYPE) bson_iter_int64(bsonit);                           \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE));  \
        return 1;                                                            \
    } else if (BSON_ITER_HOLDS_DOUBLE(bsonit)) {                             \
        value = (NUMTYPE) bson_iter_double(bsonit);                          \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE));  \
        return 1;                                                            \
    } else {                                                                 \
        return 0;                                                            \
    }                                                                        \
}

// Signed integers
MONARY_DEFINE_INT_LOADER(monary_load_int8_value, int8_t)
MONARY_DEFINE_INT_LOADER(monary_load_int16_value, int16_t)
MONARY_DEFINE_INT_LOADER(monary_load_int32_value, int32_t)
MONARY_DEFINE_INT_LOADER(monary_load_int64_value, int64_t)

// Unsigned integers
MONARY_DEFINE_INT_LOADER(monary_load_uint8_value, uint8_t)
MONARY_DEFINE_INT_LOADER(monary_load_uint16_value, uint16_t)
MONARY_DEFINE_INT_LOADER(monary_load_uint32_value, uint32_t)
MONARY_DEFINE_INT_LOADER(monary_load_uint64_value, uint64_t)

int monary_load_datetime_value(const bson_iter_t* bsonit,
                               monary_column_item* citem,
                               int idx)
{
    int64_t value;

    if (BSON_ITER_HOLDS_DATE_TIME(bsonit)) {
        value = bson_iter_date_time(bsonit);
        memcpy(((int64_t*) citem->storage) + idx, &value, sizeof(int64_t));
        return 1;
    } else {
        return 0;
    }
}

int monary_load_timestamp_value(const bson_iter_t* bsonit,
                                monary_column_item* citem,
                                int idx)
{
    uint32_t timestamp;
    uint32_t increment;
    char* dest;         // Would be void*, but Windows compilers complain

    dest = (char*) citem->storage + idx*sizeof(int64_t);
    if (BSON_ITER_HOLDS_TIMESTAMP(bsonit)) {
        bson_iter_timestamp(bsonit, &timestamp, &increment);
        memcpy(dest, &timestamp, sizeof(int32_t));
        memcpy(dest + sizeof(int32_t), &increment, sizeof(int32_t));
        return 1;
    } else {
        return 0;
    }
}

int monary_load_string_value(const bson_iter_t* bsonit,
                             monary_column_item* citem,
                             int idx)
{
    char* dest;         // Pointer to the final location of the array in mem
    const char* src;    // Pointer to immutable buffer
    int size;
    uint32_t stringlen; // The size of the string according to iter_utf8

    if (BSON_ITER_HOLDS_UTF8(bsonit)) {
        src = bson_iter_utf8(bsonit, &stringlen);
        // increment string len to count the null character
        stringlen++;
        size = citem->type_arg;
        if (stringlen > size) {
            stringlen = size;
        }
        dest = ((char*) citem->storage) + (idx * size);
        // Note: numpy strings need not end in \0
        memcpy(dest, src, stringlen);
        return 1;
    } else {
        return 0;
    }
}

int monary_load_binary_value(const bson_iter_t* bsonit,
                             monary_column_item* citem,
                             int idx)
{
    bson_subtype_t subtype;
    const uint8_t* binary;
    int size;
    uint32_t binary_len;
    uint8_t* dest;

    if (BSON_ITER_HOLDS_BINARY(bsonit)) {
        // Load the binary
        bson_iter_binary(bsonit, &subtype, &binary_len, &binary);

        // Size checking
        size = citem->type_arg;
        if(binary_len > size) {
            binary_len = size;
        }

        dest = ((uint8_t*) citem->storage) + (idx * size);
        memcpy(dest, binary, binary_len);
        return 1;
    } else {
        return 0;
    }
}

int monary_load_document_value(const bson_iter_t* bsonit,
                               monary_column_item* citem,
                               int idx)
{
    uint32_t document_len;      // The length of document in bytes.
    const uint8_t* document;    // Pointer to the immutable document buffer.
    uint8_t* dest;

    if (BSON_ITER_HOLDS_DOCUMENT(bsonit)) {
        bson_iter_document(bsonit, &document_len, &document);
        if (document_len > citem->type_arg) {
            document_len = citem->type_arg;
        }

        dest = ((uint8_t*) citem->storage) + (idx * document_len);
        memcpy(dest, document, document_len);
        return 1;
    } else {
        return 0;
    }
}

int monary_load_type_value(const bson_iter_t* bsonit,
                           monary_column_item* citem,
                           int idx) {
    uint8_t type;
    uint8_t* dest;
    type = (uint8_t) bson_iter_type(bsonit);
    dest = ((uint8_t*) citem->storage) + idx;
    memcpy(dest, &type, sizeof(uint8_t));
    return 1;
}

int monary_load_size_value(const bson_iter_t* bsonit,
                           monary_column_item* citem,
                           int idx)
{
    bson_type_t type;
    const uint8_t* discard;
    uint32_t size;
    uint32_t* dest;

    type = bson_iter_type(bsonit);
    switch (type) {
        case BSON_TYPE_UTF8:
        case BSON_TYPE_CODE:
            bson_iter_utf8(bsonit, &size);
            size++;
            break;
        case BSON_TYPE_BINARY:
            bson_iter_binary(bsonit, NULL, &size, &discard);
            break;
        case BSON_TYPE_DOCUMENT:
            bson_iter_document(bsonit, &size, &discard);
            break;
        case BSON_TYPE_ARRAY:
            bson_iter_array(bsonit, &size, &discard);
            break;
        default:
            return 0;
    }
    dest = ((uint32_t*) citem->storage) + idx;
    memcpy(dest, &size, sizeof(uint32_t));
    return 1;
}

int monary_load_length_value(const bson_iter_t* bsonit,
                             monary_column_item* citem,
                             int idx)
{
    bson_type_t type;
    bson_iter_t child;
    const char* discard;
    uint32_t length;
    uint32_t* dest;

    type = bson_iter_type(bsonit);
    switch (type) {
        case BSON_TYPE_UTF8:
        case BSON_TYPE_CODE:
            discard = bson_iter_utf8(bsonit, &length);
            break;
        case BSON_TYPE_ARRAY:
        case BSON_TYPE_DOCUMENT:
            if (!bson_iter_recurse(bsonit, &child)) {
                return 0;
            }
            for (length = 0; bson_iter_next(&child); length++);
            break;
        case BSON_TYPE_BINARY:
            bson_iter_binary(bsonit, NULL, &length, (const uint8_t**) &discard);
            break;
        default:
            return 0;
    }

    dest = ((uint32_t*) citem->storage) + idx;
    memcpy(dest, &length, sizeof(uint32_t));
    return 1;
}

#define MONARY_DISPATCH_TYPE(TYPENAME, TYPEFUNC)    \
case TYPENAME:                                      \
success = TYPEFUNC(bsonit, citem, offset);          \
break;

int monary_load_item(const bson_iter_t* bsonit,
                     monary_column_item* citem,
                     int offset)
{
    int success = 0;

    switch(citem->type) {
        MONARY_DISPATCH_TYPE(TYPE_OBJECTID, monary_load_objectid_value)
        MONARY_DISPATCH_TYPE(TYPE_DATE, monary_load_datetime_value)
        MONARY_DISPATCH_TYPE(TYPE_TIMESTAMP, monary_load_timestamp_value)
        MONARY_DISPATCH_TYPE(TYPE_BOOL, monary_load_bool_value)

        MONARY_DISPATCH_TYPE(TYPE_INT8, monary_load_int8_value)
        MONARY_DISPATCH_TYPE(TYPE_INT16, monary_load_int16_value)
        MONARY_DISPATCH_TYPE(TYPE_INT32, monary_load_int32_value)
        MONARY_DISPATCH_TYPE(TYPE_INT64, monary_load_int64_value)

        MONARY_DISPATCH_TYPE(TYPE_UINT8, monary_load_uint8_value)
        MONARY_DISPATCH_TYPE(TYPE_UINT16, monary_load_uint16_value)
        MONARY_DISPATCH_TYPE(TYPE_UINT32, monary_load_uint32_value)
        MONARY_DISPATCH_TYPE(TYPE_UINT64, monary_load_uint64_value)

        MONARY_DISPATCH_TYPE(TYPE_FLOAT32, monary_load_float32_value)
        MONARY_DISPATCH_TYPE(TYPE_FLOAT64, monary_load_float64_value)

        MONARY_DISPATCH_TYPE(TYPE_STRING, monary_load_string_value)
        MONARY_DISPATCH_TYPE(TYPE_BINARY, monary_load_binary_value)
        MONARY_DISPATCH_TYPE(TYPE_BSON, monary_load_document_value)

        MONARY_DISPATCH_TYPE(TYPE_SIZE, monary_load_size_value)
        MONARY_DISPATCH_TYPE(TYPE_LENGTH, monary_load_length_value)
        MONARY_DISPATCH_TYPE(TYPE_TYPE, monary_load_type_value)
        default:
            DEBUG("%s does not match any Monary type", citem->field);
            break;
    }

    return success;
}

/**
 * Copies over raw BSON data into Monary column storage. This function
 * determines the types of the data, dispatches to an appropriate handler and
 * copies over the data. It keeps a count of any unsuccessful loads and sets
 * NumPy-compatible masks on the data as appropriate.
 *
 * @param coldata A pointer to monary_column_data which contains the final
 * storage location for the BSON data.
 * @param row The row number to store the data in. Cannot exceed
 * coldata->num_rows.
 * @param bson_data A pointer to an immutable BSON data buffer.
 *
 * @return The number of unsuccessful loads.
 */
int monary_bson_to_arrays(monary_column_data* coldata,
                          unsigned int row,
                          const bson_t* bson_data)
{
    bson_iter_t bsonit;
    const char* field;
    int i;
    int success;
    int fields_found;
    monary_column_item* citem;

    if (!coldata || !bson_data) {
        DEBUG("%s", "Array pointer or BSON data was NULL and could not be loaded.");
        return -1;
    }
    if (row > coldata->num_rows) {
        DEBUG("Tried to load row %d, but that exceeds the maximum # of rows (%d)", row, coldata->num_rows);
        return -1;
    }
    if (!bson_iter_init(&bsonit, bson_data)) {
        DEBUG("%s", "Failed to initialize a BSON iterator");
        return -1;
    }

    // Mask all the values by default
    for (i = 0; i < coldata->num_columns; i++) {
        citem = coldata->columns + i;
        if (citem->mask != NULL) {
            citem->mask[row] = 1;
        }
    }

    fields_found = 0;
    while (bson_iter_next(&bsonit) && fields_found < coldata->num_columns) {
        field = bson_iter_key(&bsonit);
        success = 0;

        // Find the corresponding column
        for (i = 0; i < coldata->num_columns; i++) {
            citem = coldata->columns + i;

            // Load the item, whose type should match the storage specified
            if (strcmp(field, citem->field) == 0) {
                success = monary_load_item(&bsonit, citem, row);
                ++fields_found;

                // Record success in the mask, if applicable
                if (citem->mask != NULL) {
                    citem->mask[row] = !success;
                }
                break;
            }
        }
    }

    return coldata->num_columns - fields_found;
}

/**
 * Performs a count query on a MongoDB collection.
 *
 * @param collection The MongoDB collection to query against.
 * @param query A pointer to a BSON buffer representing the query.
 *
 * @return If unsuccessful, returns -1; otherwise, returns the number of
 * documents counted.
 */
int64_t monary_query_count(mongoc_collection_t* collection,
                           const uint8_t* query)
{
    bson_error_t error;     // A location for BSON errors
    bson_t query_bson;      // The query converted to BSON format
    int64_t total_count;    // The number of documents counted
    uint32_t query_size;    // Length of the query in bytes

    DEBUG("%s", "Starting Monary count");

    // build BSON query data
    memcpy(&query_size, query, sizeof(uint32_t));
    if (!bson_init_static(&query_bson,
                          query,
                          query_size)) {
        DEBUG("%s", "Failed to initialize raw BSON query");
        return -1;
    }
    
    // Make the count query
    total_count = mongoc_collection_count(collection,
                                          MONGOC_QUERY_NONE,
                                          &query_bson,
                                          0,
                                          0,
                                          NULL,
                                          &error);
    bson_destroy(&query_bson);
    if (total_count < 0) {
        DEBUG("error: %d.%d %s", error.domain, error.code, error.message);
    }

    return total_count;
}

/**
 * Given pre-allocated array data that specifies the fields to find, this
 * builds a BSON document that can be passed into a MongoDB query.
 *
 * @param coldata A pointer to a monary_column_data, which should have already
 * been allocated and built properly. The names of the fields of its column
 * items become the names of the fields to query for.
 * @param fields_bson A pointer to a bson_t that should already be initialized.
 * After this BSON is written to, it may be used in a query and then destroyed
 * afterwards.
 */
void monary_get_bson_fields_list(monary_column_data* coldata,
                                 bson_t* fields_bson)
{
    int i;
    monary_column_item* col;

    // We want to select exactly each field specified in coldata, of which
    // there are exactly coldata.num_columns
    for (i = 0; i < coldata->num_columns; i++) {
        col = coldata->columns + i;
        bson_append_int32(fields_bson, col->field, -1, 1);
    }
}

/**
 * Performs a find query on a MongoDB collection, selecting certain fields from
 * the results and storing them in Monary columns.
 *
 * @param collection The MongoDB collection to query against.
 * @param offset The number of documents to skip, or zero.
 * @param limit The maximum number of documents to return, or zero.
 * @param query A pointer to a BSON buffer representing the query.
 * @param coldata The column data to store the results in.
 * @param select_fields If truthy, select exactly the fields from the database
 * that match the fields in coldata. If false, the query will find and return
 * all fields from matching documents.
 *
 * @return If successful, a Monary cursor that should be freed with
 * monary_close_query() when no longer in use. If unsuccessful, or if an
 * invalid query was passed in, NULL is returned.
 */
monary_cursor* monary_init_query(mongoc_collection_t* collection,
                                 uint32_t offset,
                                 uint32_t limit,
                                 const uint8_t* query,
                                 monary_column_data* coldata,
                                 int select_fields)
{
    bson_t query_bson;          // BSON representing the query to perform
    bson_t* fields_bson;        // BSON holding the fields to select
    int32_t query_size;
    monary_cursor* cursor;
    mongoc_cursor_t* mcursor;   // A MongoDB cursor

    // Sanity checks
    if (!collection || !query || !coldata) {
        DEBUG("%s", "Given a NULL param.");
        return NULL;
    }

    // build BSON query data
    memcpy(&query_size, query, sizeof(int32_t));
    query_size = (int32_t) BSON_UINT32_FROM_LE(query_size);
    if (!bson_init_static(&query_bson,
                          query,
                          query_size)) {
        DEBUG("%s", "Failed to initialize raw BSON query");
        return NULL;
    }
    fields_bson = NULL;


    // build BSON fields list (if necessary)
    if(select_fields) {
        fields_bson = bson_new();
        if (!fields_bson) {
            DEBUG("%s", "An error occurred while allocating memory for BSON data");
            return NULL;
        }
        monary_get_bson_fields_list(coldata, fields_bson);
    }

    // create query cursor
    mcursor = mongoc_collection_find(collection,
                                     MONGOC_QUERY_NONE,
                                     offset,
                                     limit,
                                     0,
                                     &query_bson,
                                     fields_bson,
                                     NULL);

    // destroy BSON fields
    bson_destroy(&query_bson);
    if(fields_bson) { bson_destroy(fields_bson); }

    if (!mcursor) {
        DEBUG("%s", "An error occurred with the query");
        return NULL;
    }

    // finally, create a new Monary cursor
    cursor = (monary_cursor*) malloc(sizeof(monary_cursor));
    cursor->mcursor = mcursor;
    cursor->coldata = coldata;
    return cursor;
}

/**
 * Grabs the results obtained from the MongoDB cursor and loads them into
 * in-memory arrays.
 *
 * @param cursor A pointer to a Monary cursor, which contains both a MongoDB
 * cursor and Monary column data that stores the retrieved information.
 *
 * @return The number of rows loaded into memory.
 */
int monary_load_query(monary_cursor* cursor)
{
    bson_error_t error;             // A location for errors
    const bson_t* bson;             // Pointer to an immutable BSON buffer
    int num_masked;
    int row;
    int total_values;
    monary_column_data* coldata;
    mongoc_cursor_t* mcursor;

    mcursor = cursor->mcursor;  // The underlying MongoDB cursor
    coldata = cursor->coldata;  // A pointer to the NumPy array data
    row = 0;                    // Iterator var over the lengths of the arrays
    num_masked = 0;             // The number of failed loads
    
    // read result values
    while(row < coldata->num_rows
            && !mongoc_cursor_error(mcursor, &error)
            && mongoc_cursor_next(mcursor, &bson)) {

#ifndef NDEBUG
        if(row % 500000 == 0) {
            DEBUG("...%i rows loaded", row);
        }
#endif

        num_masked += monary_bson_to_arrays(coldata, row, bson);
        ++row;
    }

    if (mongoc_cursor_error(mcursor, &error)) {
        DEBUG("error: %d.%d %s", error.domain, error.code, error.message);
    }

    total_values = row * coldata->num_columns;
    DEBUG("%i rows loaded; %i / %i values were masked", row, num_masked, total_values);

    return row;
}

/**
 * Destroys the underlying MongoDB cursor associated with the given cursor.
 *
 * Note that the column data is not freed in this function as that data is
 * exposed as NumPy arrays in Python.
 *
 * @param cursor A pointer to the Monary cursor to close. If cursor is NULL,
 * no operation is performed.
 */
void monary_close_query(monary_cursor* cursor)
{
    if (cursor) {
        DEBUG("%s", "Closing query");
        mongoc_cursor_destroy(cursor->mcursor);
        free(cursor);
    }
}


#define MONARY_SET_BSON_VALUE(TYPENAME, BTYPENAME, VKEY, STORED_TYPE, CAST_TYPE) \
case TYPENAME:                                                                   \
val->value_type = BTYPENAME;                                                     \
val->value.VKEY = (CAST_TYPE) *(((STORED_TYPE *) citem->storage) + idx);         \
success = 1;                                                                     \
break;

/**
 * Create a bson_value_t from the given monary column and row
 *
 * @param val A pointer to the bson_value_t to populate.
 * @param citem The monary column that contains the value to use
 * @param idx The index of the storage to use.
 *
 * @return 1 if successful, 0 otherwise
 */
int monary_make_bson_value_t(bson_value_t* val,
                             monary_column_item* citem,
                             int idx)
{
    uint32_t len;
    int success = 0;
    val->padding = 0;
    switch (citem->type) {
        MONARY_SET_BSON_VALUE(TYPE_BOOL, BSON_TYPE_BOOL, v_bool, bool, bool)
        MONARY_SET_BSON_VALUE(TYPE_INT8, BSON_TYPE_INT32,
                              v_int32, int8_t, int32_t)
        MONARY_SET_BSON_VALUE(TYPE_INT16, BSON_TYPE_INT32,
                              v_int32, int16_t, int32_t)
        MONARY_SET_BSON_VALUE(TYPE_INT32, BSON_TYPE_INT32,
                              v_int32, int32_t, int32_t)
        MONARY_SET_BSON_VALUE(TYPE_INT64, BSON_TYPE_INT64,
                              v_int64, int64_t, int64_t)
        MONARY_SET_BSON_VALUE(TYPE_UINT8, BSON_TYPE_INT32,
                              v_int32, uint8_t, int32_t)
        MONARY_SET_BSON_VALUE(TYPE_UINT16, BSON_TYPE_INT32,
                              v_int32, uint16_t, int32_t)
        MONARY_SET_BSON_VALUE(TYPE_UINT32, BSON_TYPE_INT32,
                              v_int32, uint32_t, int32_t)
        MONARY_SET_BSON_VALUE(TYPE_UINT64, BSON_TYPE_INT64,
                              v_int64, uint64_t, int64_t)
        MONARY_SET_BSON_VALUE(TYPE_FLOAT32, BSON_TYPE_DOUBLE,
                              v_double, float, double)
        MONARY_SET_BSON_VALUE(TYPE_FLOAT64, BSON_TYPE_DOUBLE,
                              v_double, double, double)
        MONARY_SET_BSON_VALUE(TYPE_DATE, BSON_TYPE_DATE_TIME,
                              v_datetime, int64_t, int64_t)
        case TYPE_OBJECTID:
            val->value_type = BSON_TYPE_OID;
            bson_oid_init_from_data(&(val->value.v_oid),
                                    ((uint8_t*) citem->storage) +
                                     (idx * sizeof(bson_oid_t)));
            success = 1;
            break;
        case TYPE_TIMESTAMP:
            val->value_type = BSON_TYPE_TIMESTAMP;
            val->value.v_timestamp.timestamp = ((uint32_t*) citem->storage)[2 * idx];
            val->value.v_timestamp.increment = ((uint32_t*) citem->storage)[(2 * idx) + 1];;
            success = 1;
            break;
        case TYPE_STRING:
            val->value_type = BSON_TYPE_UTF8;
            val->value.v_utf8.len = citem->type_arg;
            val->value.v_utf8.str = ((char*) citem->storage) +
                                    (idx * citem->type_arg);
            success = 1;
            break;
        case TYPE_BINARY:
            val->value_type = BSON_TYPE_BINARY;
            val->value.v_binary.subtype = BSON_SUBTYPE_BINARY;
            val->value.v_binary.data_len = citem->type_arg;
            val->value.v_binary.data = ((uint8_t*) citem->storage) +
                                       (idx * citem->type_arg);
            success = 1;
            break;
        case TYPE_BSON:
            len = *(uint32_t*)(((uint8_t*) citem->storage) +
                               (idx * citem->type_arg));
            if (len > citem->type_arg) {
                DEBUG("Error: bson length greater than array width in "
                      "row %d", idx);
                break;
            }
            if (len < 5) {
                DEBUG("Error: poorly formatted bson in row %d", idx);
                break;
            }
            val->value_type = BSON_TYPE_DOCUMENT;
            val->value.v_doc.data_len = len;
            val->value.v_doc.data = ((uint8_t*) citem->storage) +
                                    (idx * citem->type_arg);
            success = 1;
            break;
        default:
            DEBUG("Unsupported type %d", citem->type);
    }
    return success;
}

/**
 * Creates the bson document @parent from the given columns.
 *
 * @param columns A list of monary column items storing the values to insert
 * @param row The row in which the current data is stored
 * @param col_start The column at which to start when appending values
 * @param col_end The column at which to end when appending values
 * @param parent The bson document to append to
 * @param name_offset Offset into the field name (for nested documents)
 * @param depth Number of recursive calls made
 *
 * @return 1 if successful, 0 otherwise
 */
int monary_bson_from_columns(monary_column_item* columns,
                             int row,
                             int col_start,
                             int col_end,
                             bson_t* parent,
                             int name_offset,
                             int depth){
    bson_t child;
    bson_value_t val;
    char *field;
    monary_column_item* citem;
    int dot_idx;
    int i;
    int new_end;

    if (depth >= MONARY_MAX_RECURSION) {
        DEBUG("Max recursive depth (%d) exceed on row: %d",
              MONARY_MAX_RECURSION, row);
        return 0;
    }
    for (i = col_start; i < col_end; i++) {
        citem = columns + i;
        if (! *(citem->mask + row)) {
            // only append unmasked values
            dot_idx = 0;
            for (field = citem->field + name_offset;
                 *(field + dot_idx) && (field[dot_idx] != '.');
                 dot_idx++)
            ; // Advance dot_idx to either '.' or '\0'
            if (*(field + dot_idx)) {
                for (new_end = i + 1;
                     new_end < col_end
                     && strlen((columns + new_end)->field) > name_offset + dot_idx
                     && (strncmp(field,
                                 (columns + new_end)->field + name_offset,
                                 dot_idx) == 0);
                     new_end++)
                ; // Advance new_end so everything before has the same key
                bson_append_document_begin(parent, citem->field + name_offset,
                                           dot_idx, &child);
                monary_bson_from_columns(columns, row, i, new_end, &child,
                                         name_offset + dot_idx + 1, depth + 1);
                bson_append_document_end(parent, &child);
                i = new_end - 1;
            } else {
                if(monary_make_bson_value_t(&val, citem, row)) {
                    bson_append_value(parent, citem->field + name_offset,
                                      dot_idx, &val);
                } else {
                    DEBUG("Insert does not support Monary type %d.",
                          citem->type);
                    return 0;
                }
            }
        }
    }
    return 1;
}

/**
 * Puts the given data into BSON and inserts into the given collection.
 *
 * @param collection The MongoDB collection to insert to.
 * @param coldata The column data storing the values to insert.
 * @param client The connection to the database.
 *
 * @return The number of documents inserted into the database.
 */
int monary_insert(mongoc_collection_t* collection,
                  monary_column_data* coldata,
                  mongoc_client_t* client)
{
    bson_error_t error;
    bson_iter_t bsonit;
    bson_iter_t descendant;
    bson_oid_t oid;
    bson_t document;
    bson_t reply;
    monary_column_item* citem;
    monary_column_item* id_citem;
    mongoc_bulk_operation_t* bulk_op;
    int base_len;
    int data_len;
    int i;
    int len;
    int max_message_size;
    int num_inserted;
    int row;
    uint32_t num_docs;

    // Sanity checks
    if (!collection || !coldata) {
        DEBUG("%s", "Given a NULL param.");
        return 0;
    }

    id_citem = coldata->columns + (coldata->num_columns - 1);

    bulk_op = mongoc_collection_create_bulk_operation(collection, false, NULL);

    bson_init(&document);
    bson_init(&reply);
    num_inserted = 0;

    max_message_size = mongoc_client_get_max_message_size(client);
    DEBUG("Max messgae size: %d", max_message_size);
    base_len = 60; // TODO something like `bulk_op->commands.element_size;`
    data_len = 0;

    DEBUG("Inserting %d documents with %d keys.",
          coldata->num_rows, coldata->num_columns);
    for (row = 0; row <= coldata->num_rows; row++) {
        if (row != coldata->num_rows) {
            if (strcmp(coldata->columns->field, "_id") != 0) {
                // If not _id is provided, generate one.
                bson_oid_init(&oid, NULL);
                BSON_APPEND_OID(&document, "_id", &oid);
            }
            if (!monary_bson_from_columns(coldata->columns, row, 0,
                                      coldata->num_columns - 1, &document,
                                      0, 0)) {
                num_inserted *= -1;
                goto end;
            }
        }

        if ((data_len + base_len + document.len > max_message_size) ||
            (row == coldata->num_rows)) {
            num_docs = row - num_inserted;
            DEBUG("Inserting documents %d through %d, total data: %d",
                  num_inserted + 1, row, data_len + base_len);
            if (mongoc_bulk_operation_execute(bulk_op, &reply, &error)) {
                num_inserted += num_docs;
                data_len = 0;
            } else {
                DEBUG("Failed to insert documents %d through %d",
                      num_inserted + 1, row);
                if (error.message) {
                    DEBUG("Error message: %s", error.message);
                }
                num_inserted *= -1;
                goto end;
            }
            mongoc_bulk_operation_destroy(bulk_op);
            bulk_op = mongoc_collection_create_bulk_operation(collection,
                                                              false, NULL);
            bson_reinit(&reply);
        }

        data_len += document.len;
        mongoc_bulk_operation_insert(bulk_op, &document);

        if (row != coldata->num_rows) {
            // Load _id's for the user.
            bson_iter_init(&bsonit, &document);
            if (bson_iter_find_descendant(&bsonit, id_citem->field, &descendant)) {
                if (!monary_load_item(&descendant, id_citem, row)) {
                    num_inserted *= -1;
                    goto end;
                }
            } else {
                num_inserted *= -1;
                goto end;
            }
        }

        bson_reinit(&document);
    }
    DEBUG("Inserted %d of %d documents", num_inserted, coldata->num_rows);
end:
    bson_destroy(&document);
    bson_destroy(&reply);
    mongoc_bulk_operation_destroy(bulk_op);
    return num_inserted;
}
