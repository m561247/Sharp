//
// Created by bknun on 7/2/2022.
//

#ifndef SHARP_TRY_CATCH_DATA_H
#define SHARP_TRY_CATCH_DATA_H

#include "../../../../stdimports.h"
#include "../meta_data.h"

struct context;
struct operation_schema;
struct sharp_tc_data;

struct finally_data {
    finally_data()
            :
            id(-1)
    {}

    finally_data(Int id, sharp_tc_data *parent)
            :
            id(id),
            parent(parent)
    {
    }

    finally_data(const finally_data &tc_data)
            :
            id(tc_data.id),
            parent(tc_data.parent)
    {}

    ~finally_data()
    {
    }

    Int id;
    sharp_tc_data *parent;
};

struct catch_data {
    catch_data()
            :
            id(-1)
    {}

    catch_data(Int id, sharp_tc_data *parent)
            :
            id(id),
            parent(parent)
    {
    }

    catch_data(const catch_data &tc_data)
            :
            id(tc_data.id),
            parent(tc_data.parent)
    {}

    ~catch_data()
    {
    }

    Int id;
    sharp_tc_data *parent;
};

struct sharp_tc_data {
    sharp_tc_data()
            :
            id(-1)
    {}

    sharp_tc_data(Int id)
            :
            id(id)
    {
    }

    sharp_tc_data(const sharp_tc_data &tc_data)
            :
            id(tc_data.id)
    {}

    ~sharp_tc_data()
    {
    }

    Int id;
};

sharp_tc_data* create_try_catch_data_tracker(
        operation_schema *scheme);

catch_data* create_catch_data_tracker(
        sharp_tc_data *parent,
        operation_schema *scheme);

finally_data* create_finally_data_tracker(
        sharp_tc_data *parent,
        operation_schema *scheme);

#endif //SHARP_TRY_CATCH_DATA_H