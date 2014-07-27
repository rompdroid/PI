///////////////////////////////////////////////////////////////////////////////////////////////////
//
// stable.h
// Copyright (c) 2008 by Xie Yun. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _H_STABLE
#define _H_STABLE

#include "stree.h"

// STable
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _field, int _col >
struct STableStruct
    : public STreeStruct<_field, _col>
{
    typedef _field Field;
    typedef Field * FieldPtr;
    typedef struct Row;
    typedef Row * RowPtr;
    typedef struct RowEntry;
    typedef RowEntry * RowEntryPtr;
    typedef typename RowEntryPtr REPtr;

    struct Row
    {
        Field m_field[_col];
    };

    struct RowEntry
        : public Row
    {
        REPtr m_field_next[_col];
    };

    enum {
        columns = _col,
        row_size = sizeof(Row),
        row_size_2 = sizeof(RowEntry),
        row_size_1 = row_size_2 - row_size,
    };
};

template< class _container >
class STableIterator
    : public STreeIterator<_container> // GTableIterator<_container>
{
public:
    typedef STableIterator<_container> Self;
    typedef STreeIterator<_container> Super;
    typedef typename _container::Field Field;
    typedef typename _container::Row Row;
    typedef typename _container::RowPtr RowPtr;
    typedef typename _container::RowEntry RE;
    typedef typename _container::RowEntryPtr REPtr;

    STableIterator( RowPtr ptr = 0 )
        : Super( ptr )
        {};

    STableIterator( const Self & rhs )
        : Super( rhs )
        {};

    using Super::operator*;
    using Super::operator->;
    using Super::get;

    template< unsigned int _fn > RowPtr exists( const Field f ) const throw()
    {
        if ( this->ended() )
            return 0;

        REPtr ptr = this->m_ptr;
        while ( ptr && ptr->m_field[_fn] != f )
        {
            ptr = ptr->m_field_next[_fn];
        }
        return ptr;
    };
};

template< class _field,
    int _col,
    class _alloc = ObjectAllocator >
class STable 
    : public STree<typename STableStruct<_field, _col>::Row, _col, _alloc>
{
    friend class GTable<STableStruct<_field, _col>, _alloc>;
public:
    typedef STable<_field, _col> Self;
    typedef STableStruct<_field, _col> Struct;
    typedef STree<typename Struct::Row, _col, _alloc> Super;
    typedef Super::Argument Argument;
    typedef typename Struct::Field Field;
    typedef typename Struct::FieldPtr FieldPtr;
    typedef typename Struct::Row Row;
    typedef typename Struct::RowPtr RowPtr;
    typedef typename Struct::RowEntryPtr RowEntryPtr;
    typedef typename Struct::REPtr REPtr;
    typedef typename STableIterator<Self> FieldIterator, Fit;
    typedef typename Super::RowIterator RowIterator, Rit;

    STable( UInt32 count = 0,
        UInt8 align = row_align_none,
        Super::AllocPtr p_alloc = NULL )
        : Super( count, align, p_alloc )
        {};

    STable( const Self & rhs ) throw()
        : Super( rhs )
        {};

    explicit STable( const Argument & arg )
        : Super( arg )
        {};

    ~STable() throw()
        {
            this->Super::~Super();
        };
};

#endif // _H_STABLE
