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
    TYPE_DATE = 13,     // BSON date-time, seconds since the UNIX epoch (uint64 storage)
    TYPE_STRING = 14,   // each record is (type_arg) chars in length
    TYPE_BINARY = 15,   // each record is (type_arg) bytes in length
    TYPE_BSON = 16,     // BSON subdocument as binary; each record is type_arg bytes
    TYPE_TYPE = 17,
    TYPE_SIZE = 18,
    TYPE_LENGTH = 19,
    LAST_TYPE = 19,
};

/**
 * Makes a new connection to a MongoDB server and database.
 *
 * @param uri A MongoDB URI, as per mongoc_uri(7).
 *
 * @return A pointer to a mongoc_client_t, or NULL if the connection attempt
 * was unsuccessful.
 */
mongoc_client_t *monary_connect(const char *uri) {
    mongoc_client_t *client;
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
mongoc_collection_t *monary_use_collection(mongoc_client_t *client,
                                           const char *db,
                                           const char *collection)
{
    return mongoc_client_get_collection(client, db, collection);
}

/**
 * Destroys the given collection, allowing you to connect to another one.
 *
 * @param collection The collection to destroy.
 */
void monary_destroy_collection(mongoc_collection_t *collection)
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
    if(num_columns > MONARY_MAX_NUM_COLUMNS) { return NULL; }
    monary_column_data* result = (monary_column_data*) malloc(sizeof(monary_column_data));
    monary_column_item* columns = (monary_column_item*) calloc(num_columns, sizeof(monary_column_item));

    DEBUG("%s", "Column data allocated");

    result->num_columns = num_columns;
    result->num_rows = num_rows;
    result->columns = columns;

    return result;
}

int monary_free_column_data(monary_column_data* coldata)
{
    if(coldata == NULL || coldata->columns == NULL) { return 0; }
    for(int i = 0; i < coldata->num_columns; i++) {
        monary_column_item* col = coldata->columns + i;
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
    if(coldata == NULL) { return 0; }
    if(colnum >= coldata->num_columns) { return 0; }
    if(type == TYPE_UNDEFINED || type > LAST_TYPE) { return 0; }
    if(storage == NULL) { return 0; }
    if(mask == NULL) { return 0; }
    
    int len = strlen(field);
    if(len > MONARY_MAX_STRING_LENGTH) { return 0; }
    
    monary_column_item* col = coldata->columns + colnum;

    col->field = (char*) malloc((len + 1) * sizeof(char));
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
    if (BSON_ITER_HOLDS_OID(bsonit)) {
        const bson_oid_t* oid = bson_iter_oid(bsonit);
        uint8_t *dest = ((uint8_t *) citem->storage) + (idx * 12);
        memcpy(dest, oid->bytes, 12);
        return 1;
    } else {
        return 0;
    }
}

int monary_load_bool_value(const bson_iter_t* bsonit,
                           monary_column_item* citem,
                           int idx)
{
    bool value = bson_iter_bool(bsonit);
    memcpy(((bool *) citem->storage) + idx, &value, sizeof(bool));
    return 1;
}


#define MONARY_DEFINE_FLOAT_LOADER(FUNCNAME, NUMTYPE)                           \
int FUNCNAME (const bson_iter_t *bsonit,                                        \
              monary_column_item *citem,                                        \
              int idx)                                                          \
{                                                                               \
    NUMTYPE value;                                                              \
    if (BSON_ITER_HOLDS_DOUBLE(bsonit)) {                                       \
        value = (NUMTYPE) bson_iter_double(bsonit);                             \
        memcpy(((NUMTYPE *) citem->storage) + idx, &value, sizeof(NUMTYPE));    \
        return 1;                                                               \
    } else {                                                                    \
        return 0;                                                               \
    }                                                                           \
}

// Floating point
MONARY_DEFINE_FLOAT_LOADER(monary_load_float32_value, float);
MONARY_DEFINE_FLOAT_LOADER(monary_load_float64_value, double);

#define MONARY_DEFINE_INT_LOADER(FUNCNAME, NUMTYPE)                         \
int FUNCNAME (const bson_iter_t *bsonit,                                    \
              monary_column_item *citem,                                    \
              int idx)                                                      \
{                                                                           \
    NUMTYPE value;                                                          \
    if (BSON_ITER_HOLDS_INT32(bsonit)) {                                    \
        value = (NUMTYPE) bson_iter_int32(bsonit);                          \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE)); \
        return 1;                                                           \
    } else if (BSON_ITER_HOLDS_INT64(bsonit)) {                             \
        value = (NUMTYPE) bson_iter_int64(bsonit);                          \
        memcpy(((NUMTYPE*) citem->storage) + idx, &value, sizeof(NUMTYPE)); \
        return 1;                                                           \
    } else {                                                                \
        return 0;                                                           \
    }                                                                       \
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
    if (BSON_ITER_HOLDS_DATE_TIME(bsonit)) {
        int64_t value = bson_iter_date_time(bsonit);
        memcpy(((int64_t *) citem->storage) + idx, &value, sizeof(int64_t));
        return 1;
    } else {
        return 0;
    }
}

int monary_load_string_value(const bson_iter_t* bsonit,
                             monary_column_item* citem,
                             int idx)
{
    char *dest;         // A pointer to the final location of the array in mem
    const char *src;    // Pointer to immutable buffer
    uint32_t stringlen; // The size of the string according to iter_utf8
    int size;

    if (BSON_ITER_HOLDS_UTF8(bsonit)) {
        src = bson_iter_utf8(bsonit, &stringlen);
        stringlen++;
        size = citem->type_arg;
        if (stringlen > size) {
            stringlen = size;
        }
        dest = ((char*) citem->storage) + (idx * size);
        bson_strncpy(dest, src, stringlen);
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
    const uint8_t *binary;
    uint32_t binary_len;

    if (BSON_ITER_HOLDS_BINARY(bsonit)) {
        // Load the binary
        bson_iter_binary(bsonit, &subtype, &binary_len, &binary);

        // Size checking
        int size = citem->type_arg;
        if(binary_len > size) {
            binary_len = size;
        }

        uint8_t *dest = ((uint8_t*) citem->storage) + (idx * size);
        memcpy(dest, binary, binary_len);
        return 1;
    } else {
        return 0;
    }
}

int monary_load_document_value(const bson_iter_t *bsonit,
                               monary_column_item *citem,
                               int idx)
{
    uint32_t document_len;      // The length of document in bytes.
    const uint8_t *document;    // Pointer to the immutable document buffer.
    uint8_t *dest;

    if (BSON_ITER_HOLDS_DOCUMENT(bsonit)) {
        bson_iter_document(bsonit, &document_len, &document);
        if (document_len > citem->type_arg) {
            document_len = citem->type_arg;
        }

        dest = ((uint8_t *) citem->storage) + (idx * document_len);
        memcpy(dest, document, document_len);
        return 1;
    }
    else {
        return 0;
    }
}

int monary_load_type_value(const bson_iter_t *bsonit,
                           monary_column_item *citem,
                           int idx) {
    uint8_t type;
    uint8_t *dest;
    type = (uint8_t) bson_iter_type(bsonit);
    dest = ((uint8_t *) citem->storage) + idx;
    memcpy(dest, &type, sizeof(uint8_t));
    return 1;
}

int monary_load_size_value(const bson_iter_t *bsonit,
                           monary_column_item *citem,
                           int idx)
{
    bson_type_t type;
    const uint8_t *discard;
    uint32_t size;
    uint32_t *dest;

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
    dest = ((uint32_t *) citem->storage) + idx;
    memcpy(dest, &size, sizeof(uint32_t));
    return 1;
}

int monary_load_length_value(const bson_iter_t *bsonit,
                             monary_column_item *citem,
                             int idx)
{
    bson_type_t type;
    bson_iter_t child;
    const char *discard;
    uint32_t length;
    uint32_t *dest;

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
        default:
            return 0;
    }

    dest = ((uint32_t *) citem->storage) + idx;
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
    const char *field;
    int i;
    int success;
    int fields_found;
    monary_column_item *citem;

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
int64_t monary_query_count(mongoc_collection_t *collection,
                           const uint8_t *query)
{
    bson_error_t error;     // A location for BSON errors
    bson_t query_bson;      // The query converted to BSON format
    int64_t total_count;    // The number of documents counted
    uint32_t query_size;     // Length of the query in bytes

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

    DEBUG("Got count from libmongoc: %lld", total_count);
    return total_count;
}

/**
 * Given pre-allocated array data that specifies the fields to find, this
 * builds a BSON document that can be passed into a MongoDB query.
 * XXX: Perhaps this can be moved into query(), as it is only called there.
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
    monary_column_item *col;

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
monary_cursor* monary_init_query(mongoc_collection_t *collection,
                                 uint32_t offset,
                                 uint32_t limit,
                                 const uint8_t *query,
                                 monary_column_data *coldata,
                                 int select_fields)
{
    bson_t query_bson;          // BSON representing the query to perform
    bson_t *fields_bson;        // BSON holding the fields to select
    mongoc_cursor_t *mcursor;   // A MongoDB cursor
    int32_t query_size;

    // Sanity checks
    if (!collection || !query || !coldata) {
        return NULL;
    }

    // build BSON query data
    memcpy(&query_size, query, sizeof(int32_t));
    if (!bson_init_static(&query_bson,
                          query,
                          query_size)) {
        DEBUG("%s", "Failed to initialize raw BSON query");
        return NULL;
    }
    fields_bson = NULL;


    // build BSON fields list (if necessary)
    if(select_fields) {
        // Initialize BSON on the heap, as it will grow
        fields_bson = bson_new();
        if (!fields_bson) {
            DEBUG("%s", "An error occurred while allocating memory for BSON data");
            return NULL;
        }
        monary_get_bson_fields_list(coldata, &query_bson);
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
    monary_cursor* cursor = (monary_cursor*) malloc(sizeof(monary_cursor));
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
    const bson_t *bson;             // Pointer to an immutable BSON buffer
    int num_masked;
    int row;
    int total_values;
    monary_column_data *coldata;
    mongoc_cursor_t *mcursor;

    mcursor = cursor->mcursor;  // The underlying MongoDB cursor
    coldata = cursor->coldata;  // A pointer to the NumPy array data
    row = 0;                    // Iterator var over the lengths of the arrays
    num_masked = 0;             // The number of failed loads
    
    // read result values
    while(row < coldata->num_rows
            && !mongoc_cursor_error(mcursor, &error)
            && mongoc_cursor_more(mcursor)) {

#ifndef NDEBUG
        if(row % 500000 == 0) {
            DEBUG("...%i rows loaded", row);
        }
#endif

        if (mongoc_cursor_next(mcursor, &bson)) {
            num_masked += monary_bson_to_arrays(coldata, row, bson);
        }
        ++row;
    }

    total_values = coldata->num_columns * row;
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
