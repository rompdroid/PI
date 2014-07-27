///////////////////////////////////////////////////////////////////////////////////////////////////
//
// slist.h
// Copyright (c) 2008 by Xie Yun. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _H_SLIST
#define _H_SLIST

#ifndef _H_GTABLE
#include "gtable.h"
#endif

// SList
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _field >
struct SListStruct
{
    typedef _field Field;
    typedef Field Row;
    typedef Row * RowPtr;
    typedef struct RowEntry;
    typedef RowEntry * RowEntryPtr;
    typedef typename RowEntryPtr REPtr;

    struct RowEntry
        : public Row
    {
        typedef typename _field::Argument Argument;

        explicit RowEntry( const Argument & arg )
            : _field( arg )
            {};

        template< class _row > void operator= ( const _row & rhs )
            {
                this->_field::operator=( rhs );
            };
    };

    enum
    {
        columns = 1,
        row_size = sizeof(Row),
        row_size_2 = sizeof(RowEntry),
        row_size_1 = row_size_2 - row_size,
    };
};

template< class _field,
    class _alloc = ObjectAllocator >
class SList
    : public GTable<SListStruct<_field>, _alloc >
{
    friend class GTable<SListStruct<_field>, _alloc >;
public:
    typedef SList<_field, _alloc> Self;
    typedef GTable<SListStruct<_field>, _alloc > Super;
    typedef Super::Argument Argument;
    typedef SListStruct<_field> Struct;
    typedef typename Struct::Field Field;
    typedef typename Struct::Row Row;
    typedef typename Struct::RowPtr RowPtr;
    typedef typename Struct::REPtr REPtr;
    typedef typename Super::Iterator RowIterator;

    SList( UInt32 count = 0,
            UInt32 rpp = Super::all_in_one,
            AllocPtr p_alloc = 0 )
        : Super( count, rpp, p_alloc )
        {};
        
    SList( const Self & rhs ) throw()
        : Super( rhs )
        {};

    explicit SList( const Super::Argument & arg )
        : Super( arg )
        {};

    ~SList() throw()
        {
            this->Super::~Super();
        };

    template< class _row >
    RowPtr insert( const _row & row, RowPtr * p_last = 0 ) throw()
        {
            return this->on_insert( this->Super::insert(), row );
        };

    template< class _row >
    RowPtr rinsert( const Row & row, RowPtr * p_last = 0 ) throw()
        {
            return this->on_insert( this->Super::rinsert(), row );
        };

protected:
    template< class _row > RowPtr on_insert( REPtr r, const _row & row ) throw()
        {
            *r = row;
            return r;
        };
};

#endif // _H_SLIST
