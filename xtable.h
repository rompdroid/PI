///////////////////////////////////////////////////////////////////////////////////////////////////
//
// xtable.h
// Copyright (c) 2008 by Xie Yun.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#if ! defined( _H_XTABLE )
#define _H_XTABLE

#if ! defined( TYPE )
#include "xtypes.h"

#define TYPE \
    reinterpret_cast

#define VAR( type, x, y ) \
    type x = TYPE<type>(y)

#define VAR_HDPTR( x, y ) \
    VAR( HeadPtr, x, y )

#define VAR_PINT8( x, y ) \
    VAR( UInt8 *, x, y )

#define VAR_REPTR( x, y ) \
    VAR( RowEntryPtr, x, y )

#define HDPTR( x ) \
    TYPE<HeadPtr>( x )

#define REPTR( x ) \
    TYPE<RowEntryPtr>( x )

#endif

// table abstraction
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _container >
class GTableIterator
{
public:
    typedef GTableIterator<_container> Self;
    typedef typename _container::Row Row;
    typedef typename _container::RowPtr RowPtr;
    typedef typename _container::RowEntryPtr REPtr;
    typedef typename _container::Struct Struct;

    GTableIterator( RowPtr ptr = 0 )
        : m_ptr( TYPE<REPtr>( ptr ) )
        {};
    
    GTableIterator( const Self & rhs )
        : m_ptr( rhs.m_ptr )
        {};

    typename Struct::RowEntry & operator* () const
        {
            if ( ! this->m_ptr )
                throw std::exception( "bad_pointer" );

            return *this->m_ptr;
        };

    typename Struct::RowEntryPtr operator-> () const throw()
        {
            return (&**this);
        };

    Self & operator= ( const RowPtr ptr ) throw()
        {
            this->m_ptr = ptr;
            return *this;
        };

    Self & operator= ( const Self & rhs ) throw()
        {
            this->m_ptr = rhs.m_ptr;
            return *this;
        };

    bool operator== ( const RowPtr ptr ) throw()
        {
            return ( this->m_ptr == ptr );
        };

    bool operator== ( const Self & rhs ) throw()
        {
            return ( this->operator==( rhs.m_ptr ) );
        };

    bool operator!= ( const RowPtr ptr ) throw()
        {
            return ( this->m_ptr != ptr );
        };

    bool operator!= ( const Self & rhs ) throw()
        {
            return ( this->operator!=( rhs.m_ptr ) );
        };

    typename Struct::RowEntryPtr get() const throw()
        {
            return this->m_ptr;
        };

    Self & next() throw()
        {
            if ( this->m_ptr )
            {
                this->m_ptr = this->m_ptr->m_next;
            }
            return *this;
        };

    REPtr m_ptr;
};

template< class _table, class _table_struct >
class GTable
{
#define CALL_SELF( x ) \
    static_cast<Table *>(this)->x

public:
    typedef GTable<_table, _table_struct> Self;
    typedef struct Head;
    typedef Head * HeadPtr;
    typedef struct RowEntry;
    typedef struct RowEntry * RowEntryPtr;
    typedef typename RowEntryPtr REPtr;
    typedef _table Table;
    typedef _table_struct Struct;
    typedef typename Struct::Row Row;
    typedef typename Struct::RowPtr RowPtr;
    typedef GTableIterator<Self> Iterator;

    struct RowEntry
        : public Struct::RowEntry
    {
        REPtr m_next;
    };

    struct Head
    {
        UInt32 m_size;
        UInt32 m_count;
        RowEntry m_end;
        RowEntry m_freed;
    };

    enum {
        columns = Struct::columns,
        head_size = sizeof(Head),
        row_size = Struct::row_size,
        row_size_1 = Struct::row_size_1,
        row_size_2 = Struct::row_size_2 + sizeof(REPtr),
    };

public:
    GTable( const SBlock & block )
        : m_head( 0 )
        {
            if ( ! block.p || block.size < head_size + row_size_2 )
#if defined( OPI_THROW )
                OPI_THROW( "invalidParameter", "Table::construct" );
#else
                throw std::exception( "invalidParameter.Table::construct" );
#endif
            this->create( block.p, block.size );
        };

    //~GTable() throw();

    const SBlock block() const throw()
        {
            SBlock s_b = { this->m_head, this->m_head->m_size };
            return s_b;
        };

    UInt32 capacity() const throw()
        {
            return ( this->m_head->m_size - head_size ) / row_size_2;
        };

    UInt32 count() const throw()
        {
            return this->m_head->m_count;
        };

    bool exists( const RowPtr & p_row ) const throw()
        {
            if ( ! p_row )
                return false;

            UInt32 pos = PINT8( p_row ) - PINT8( this->m_head + 1 );
            return ( pos % row_size_2 == 0
                && pos < ( this->m_head->m_size - head_size ) );
        };

    void clear()
        {
            this->create( this->m_head, this->m_head->m_size );
        };

    Iterator erase( Iterator & it ) throw()
        {
            return this->erase( *it );
        };

    Iterator erase( typename Struct::RowEntryPtr row ) throw()
        {
            HeadPtr h = this->m_head;
            REPtr last = &h->m_end;
            REPtr prev = last;
            REPtr r = 0;
            do
            {
                if ( prev->m_next != row )
                {
                    prev = prev->m_next;
                }
                else
                {
                    r = prev->m_next;
                    break;
                }
            } while ( prev != last );

            if ( ! r )
                return Iterator( 0 );

            prev->m_next = r->m_next;
            r->m_next = h->m_freed.m_next;
            h->m_freed.m_next = r;
            h->m_count--;
            r = prev->m_next;
            if ( r == last )
                return Iterator( prev );

            return Iterator( r );
        };

    REPtr insert( const Row & row ) throw()
        {
            HeadPtr h = this->m_head;
            REPtr first = &h->m_freed;
            REPtr r = first->m_next;
            if ( r == first )
                // to expand block
                return 0;

            first->m_next = r->m_next;
            CALL_SELF( implInsert_1( r, row ) );
            first = &h->m_end;
            r->m_next = first->m_next;
            first->m_next = r;
            h->m_count++;
            return r;
        };

    REPtr rinsert( const Row & row ) throw()
        {
            HeadPtr h = this->m_head;
            REPtr first = &h->m_freed;
            REPtr r = first->m_next;
            if ( r == first )
                // to expand block
                return 0;

            first->m_next = r->m_next;
            CALL_SELF( implInsert_1( r, row ) );
            first = &h->m_end;
            REPtr last = first;
            for ( ; last->m_next != first; last = last->m_next );
            r->m_next = first;
            last->m_next = r;
            h->m_count++;
            return r;
        };

    void update() throw()
        {};

    const Iterator begin() const throw()
        {
            return Iterator( this->m_head->m_end.m_next );
        };

    const Iterator end() const throw()
        {
            return Iterator( &this->m_head->m_end );
        };

protected:
    void create( void * p_block, UInt32 size ) throw()
        {
            this->m_head = HDPTR( p_block );
            HeadPtr h = this->m_head;
            ::memset( h, 0, head_size );
            h->m_size = size;
            h->m_end.m_next = &h->m_end;
            REPtr r = &h->m_freed;
            r->m_next = REPTR( h + 1 );
            VAR_REPTR( end, PINT8( h ) + h->m_size - 2 * row_size_2 );
            r = r->m_next;
            while ( r < end )
            {
                CALL_SELF( implInitialize_1( TYPE<Struct::REPtr>( r ) ) );
                r->m_next = r + 1;
                r++;
            }
            r->m_next = &h->m_freed;
        };

private:
    HeadPtr m_head;
};

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
    {};

    enum {
        columns = 1,
        row_size = sizeof(Row),
        row_size_2 = sizeof(RowEntry),
        row_size_1 = row_size_2 - row_size,
    };
};

template< class _field >
class SList
    : public GTable<SList<_field>, SListStruct<_field> >
{
    friend class GTable<SList, SListStruct<_field> >;
public:
    typedef SList<_field> Self;
    typedef GTable<SList, SListStruct<_field> > Super;
    typedef SListStruct<_field> Struct;
    typedef typename Struct::Field Field;
    typedef typename Struct::Row Row;
    typedef typename Struct::RowPtr RowPtr;
    typedef typename Struct::REPtr REPtr;
    typedef typename Super::Iterator RowIterator;

    SList( const SBlock & block )
        : GTable<SList, SListStruct<_field> >( block )
        {};

    //virtual ~SList() throw();

protected:
    void implInitialize_1( REPtr r )
        {};

    void implInsert_1( REPtr r, const Row & row )
        {
            ::memcpy( r, &row, row_size );
        };
};

// CList - chunked slist
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _field, int _col >
struct CListStruct
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
        REPtr m_field_next;
    };

    enum {
        columns = 1,
        row_size = sizeof(Row),
        row_size_2 = sizeof(RowEntry),
        row_size_1 = row_size_2 - row_size,
        chunk_size = _col,
    };
};

template< class _container >
class CListIterator :
    public GTableIterator<_container>
{
public:
    typedef CListIterator<_container> Self;
    typedef GTableIterator<_container> Super;
    typedef typename _container::FieldPtr FieldPtr;
    typedef typename _container::Row Row;
    typedef typename _container::RowPtr RowPtr;
    typedef typename _container::RowEntry RE;
    typedef typename _container::RowEntryPtr REPtr;
    typedef typename _container::RowIterator Rit;

    enum
    {
        columns = _container::Struct::columns,
    };

    CListIterator( RowPtr ptr = 0 ) throw()
        : Super( ptr ),
        m_fptr( TYPE<FieldPtr>( ptr ) )
        {};

    CListIterator( const Self & rhs ) throw()
        : Super( rhs ),
        m_fptr( TYPE<FieldPtr>( rhs.m_ptr ) )
        {};

    using Super::operator*;
    using Super::operator->;

    Self & operator= ( const Self & rhs ) throw()
        {
            this->m_ptr = rhs.m_ptr;
            this->m_fptr = rhs.m_fptr;
        };

    bool ended() const throw()
        {
            if ( ! this->field_ended() )
                return false;

            return ( ! this->m_ptr || ! this->m_fptr );
        };

    bool field_ended() const throw()
        {
            return ( ! this->m_fptr || ! *this->m_fptr );
        };

    FieldPtr get() const throw()
    {
        return this->m_fptr;
    };

    Self & next() throw()
        {
            if ( ! this->ended() )
            {
                this->m_fptr++;
                if ( this->m_fptr >= TYPE<FieldPtr>( &this->m_ptr->m_field_next ) )
                {
                    this->m_ptr = this->m_ptr->m_field_next;
                    this->m_fptr = TYPE<FieldPtr>( this->m_ptr );
                }
            }
            return *this;
        };

    FieldPtr m_fptr;
};

template< class _field, int _col >
class CList 
    : public GTable<CList<_field, _col>, CListStruct<_field, _col> >
{
    friend class GTable<CList, CListStruct<_field, _col> >;
public:
    typedef CList<_field, _col> Self;
    typedef GTable<CList, CListStruct<_field, _col> > Super;
    typedef CListStruct<_field, _col> Struct;
    typedef typename Struct::Field Field;
    typedef typename Struct::FieldPtr FieldPtr;
    typedef typename Struct::Row Row;
    typedef typename Struct::RowPtr RowPtr;
    typedef typename Struct::RowEntryPtr RowEntryPtr;
    typedef typename Struct::REPtr REPtr;
    typedef typename CListIterator<Self> FieldIterator, Fit;
    typedef typename Super::Iterator RowIterator, Rit;

    CList( const SBlock & block )
        : GTable<CList, CListStruct<_field, _col> >( block )
      {};

    //virtual ~CList() throw();

    RowPtr insertRow( const Row & row, RowPtr * p_last = 0 ) throw()
        {
            REPtr r = this->Super::insert( row );
            if ( ! r )
                return 0;

            return this->insert_post( r, p_last );
        };

    RowPtr rinsertRow( const Row & row, RowPtr * p_last = 0 ) throw()
        {
            REPtr r = this->Super::rinsert( row );
            if ( ! r )
                return 0;

            return this->insert_post( r, p_last );
        };

protected:
    void implInitialize_1( REPtr r )
        {
            r->m_field_next = 0;
        };

    void implInsert_1( REPtr r, const Row & row )
        {
            ::memcpy( r, &row, row_size );
        };

    RowPtr insert_post( const REPtr r, RowPtr * p_last ) throw()
        {
            if ( p_last )
            {
                r->m_field_next = REPTR( *p_last );
                *p_last = r;
            }
            return r;
        };
};


// STable
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _field, int _col >
struct STableStruct
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
    : public GTableIterator<_container>
{
public:
    typedef STableIterator<_container> Self;
    typedef GTableIterator<_container> Super;
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

    bool ended() const throw()
        {
            return ( ! this->m_ptr );
        };

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

    template< unsigned int _fn > REPtr & get() const throw()
        {
            return this->m_ptr->m_field_next[_fn];
        };

    Self & next( unsigned int fn ) throw()
        {
            if ( ! this->ended() )
            {
                this->m_ptr = this->m_ptr->m_field_next[fn];
            }
            return *this;
        };

    template< unsigned int _fn > Self & next() throw()
        {
            return this->next( _fn );
        };

    template< unsigned int _fn > Self & prev( const Self & it ) throw()
    {
        if ( this->ended() )
            return *this;

        REPtr t_ptr = it.get();
        REPtr ptr = this->m_ptr;
        while ( ptr && t_ptr != TYPE<RowPtr>( ptr->m_field_next[_fn] ) )
        {
            ptr = ptr->m_field_next[_fn];
        }
        this->m_ptr = ptr;
        return *this;
    };

    template< unsigned int _fn > Self & prev( unsigned int field, const Field f ) throw()
    {
        if ( this->ended() )
            return *this;

        REPtr ptr = this->m_ptr;
        while ( ptr )
        {
            REPtr p = ptr->m_field_next[_fn];
            if ( p && p->m_field[field] == f )
                break;

            ptr = p;
        }
        this->m_ptr = ptr;
        return *this;
    };
};

template< class _field, int _col >
class STable 
    : public GTable<STable<_field, _col>, STableStruct<_field, _col> >
{
    friend class GTable<STable, STableStruct<_field, _col> >;
public:
    typedef STable<_field, _col> Self;
    typedef GTable<STable, STableStruct<_field, _col> > Super;
    typedef STableStruct<_field, _col> Struct;
    typedef typename Struct::Field Field;
    typedef typename Struct::FieldPtr FieldPtr;
    typedef typename Struct::Row Row;
    typedef typename Struct::RowPtr RowPtr;
    typedef typename Struct::RowEntryPtr RowEntryPtr;
    typedef typename Struct::REPtr REPtr;
    typedef typename STableIterator<Self> FieldIterator, Fit;
    typedef typename Super::Iterator RowIterator, Rit;

    STable( const SBlock & block )
        : Super( block )
        {};

    //virtual ~STable() throw();

    Rit erase( Rit & it, RowPtr * p_last[Struct::columns] = 0 ) throw()
        {
            return this->erase( TYPE<RowPtr>( it.get() ), p_last );
        };

    Rit erase( RowPtr ptr, RowPtr * p_last[Struct::columns] = 0 ) throw()
        {
            this->erase_pre( TYPE<RowEntryPtr>( ptr ), p_last );
            return this->Super::erase( TYPE<RowEntryPtr>( ptr ) );
        };

    RowPtr insert( const Row & row, RowPtr * p_last[Struct::columns] = 0 ) throw()
        {
            REPtr r = this->Super::insert( row );
            if ( ! r )
                return 0;

            return this->insert_post( r, p_last );
        };

    RowPtr rinsert( const Row & row, RowPtr * p_last[Struct::columns] = 0 ) throw()
        {
            REPtr r = this->Super::rinsert( row );
            if ( ! r )
                return 0;

            return this->insert_post( r, p_last );
        };

protected:
    void implInitialize_1( REPtr r )
        {
            ::memset( r->m_field_next, 0, row_size_1 );
        };

    void implInsert_1( REPtr r, const Row & row )
        {
            ::memcpy( r, &row, row_size );
        };

    void erase_pre( REPtr r, RowPtr * p_last[Struct::columns] ) throw()
        {
            if ( p_last )
            {
                REPtr * ptr = 0;
                REPtr * p_field = r->m_field_next;
                for ( UInt32 i = 0; i < Struct::columns; ++i )
                {
                    ptr = TYPE<REPtr *>( p_last[i] );
                    if ( ptr && *ptr )
                    {
                        (*ptr)->m_field_next[i] = p_field[i];
                        p_field[i] = 0;
                    }
                }
            }
            //this->implInitialize_1( r );
        };

    RowPtr insert_post( const REPtr r, RowPtr * p_last[Struct::columns] ) throw()
        {
            if ( p_last )
            {
                REPtr * p_field = r->m_field_next;
                for ( UInt32 i = 0; i < Struct::columns; ++i )
                {
                    p_field[i] = REPTR( *p_last[i] );
                    *p_last[i] = r;
                }
            }
            return r;
        };
};


// functions
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _table >
inline
_table *
createTable( UInt32 size )
{
    UInt32 sz = size * _table::row_size_2 + _table::head_size;
    SBlock s_b = { new UInt8[sz], sz };
    return new _table( s_b );
}

template< class _table >
inline
void
dropTable( _table *& p_tbl ) throw()
{
    if ( ! p_tbl )
        return;

    VAR_PINT8( p, p_tbl->block().p );
    OPI::freeArrayPtr( p );
    OPI::freePtr( p_tbl );
}

#endif // _H_XTABLE
