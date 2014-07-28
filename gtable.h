///////////////////////////////////////////////////////////////////////////////////////////////////
//
// gtable.h
// Copyright (c) 2009 by Xie Yun.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef H_GTABLE
#define H_GTABLE

#include "../allocator.h"
#include "../exception.h"
#include "gnode.h"

namespace Common
{
// GTable
//    table abstraction
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _field,
    class _alloc = default_alloc >
class GTable
{
public:
    typedef GTable<_field, _alloc> Self;
    typedef _field Field;
    typedef _alloc Allocator;
    //class _Iterator;
    //typedef _Iterator iterator;

protected:
    struct _Head;
    typedef typename Pointer<_Head>::pointer PHead;
    typedef GNode<Field, 1> _Node;
    typedef typename _Node::Nodeptr Nodeptr;
    typedef typename _Node::PNode PNode;
    typedef Field * PField;
    
    struct _Head
    {
        UInt32 m_count;
        UInt32 m_capacity;
        Pointer<_Node> m_p_used;
        Pointer<_Node> m_p_free;
    };

    enum
    {
        head_size = sizeof(_Head),
        node_size = sizeof(_Node),
    };

public:
    typedef typename PNode pointer;
    typedef typename Nodeptr node_pointer;
    typedef typename PField field_pointer;

    GTable( UInt32 count = 0, const Allocator & alloc = Allocator() ) throw()
        : m_alloc( alloc ),
          m_head()
        {
            this->initialize( count );
        }

    GTable( const SBlock & block, const Allocator & alloc ) throw()
        : m_alloc( alloc ),
          m_head( TYPE<PHead>(block.p) )
        {}

    virtual ~GTable() throw()
        {
            this->finalize();
        }

    Allocator & allocator() throw()
        {
            return this->m_alloc;
        }

    SBlock block() throw()
    {
        UInt32 sz = head_size + ( this->m_head->m_capacity + 2 ) * node_size;
        SBlock b = { this->m_head.get(), sz };
        return b;
    }

    UInt32 capacity() throw()
        {
            return this->m_head->m_capacity;
        }

    UInt32 count() throw()
        {
            return this->m_head->m_count;
        }

    PNode begin() const throw()
        {
            return this->m_head->m_p_used->next();
        }

    PNode end() const throw()
        {
            return this->m_head->m_p_used.get();
        }

    bool exists( const PNode np ) const throw()
        {
            if ( !np )
                return false;

            return ( np - this->m_head->m_p_free.get() <= this->m_head->m_count );
        }

    PNode erase( PField p ) throw()
        {
            VTYPE( PNode, ep, p );
            if ( !this->exists( ep ) )
                return 0;

            PHead h = this->m_head.get();
            PNode np = ep->next();
            if ( h->m_p_used == ep )
                return np;

            ep->unlink();
            h->m_p_free->next()->link( ep );
            ep->link( h->m_p_free.get() );
            h->m_count--;
            if ( h->m_p_used == np )
                return np->next();

            return np;
        }

    template< typename _ty >
    PNode insert( const _ty & v )
        {
            PNode np = this->insert();
            this->m_alloc.construct<Field>( np, v );
            return np;
        }

    PNode insert()
        {
            PHead h = this->m_head.get();
            if ( this->q_empty( h->m_p_free.get() ) )
            {
                this->expand();
            }
            PNode np = h->m_p_free->next();
            np->unlink();
            np->link( h->m_p_used->prior() );
            h->m_p_used->link( np );
            h->m_count++;
            return np;
        }

    template< typename _ty >
    PNode rinsert( const _ty & v )
        {
            PNode np = this->rinsert();
            this->m_alloc.construct<_ty>( np, v );
            return np;
        }
    
    PNode rinsert()
        {
            PHead h = this->m_head.get();
            if ( this->q_empty( h->m_p_free.get() ) )
            {
                this->expand();
            }
            PNode np = h->m_p_free->next();
            np->unlink();
            h->m_p_used->next()->link( np );
            np->link( h->m_p_used.get() );
            h->m_count++;
            return np;
        }

protected:
    GTable( const Self & rhs ) throw()
        : m_alloc( rhs.m_alloc )
          m_head( 0 )
        {};

    void expand()
        {
            PHead h = this->m_head.get();
            PNode np = this->m_alloc.allocate<_Node>( h->m_capacity );
            if ( !np )
                throw Exception( "memory insufficient", "GTable::expand" );

            h->m_p_free = np;
            PNode np1 = np;
            for ( UInt32 i = 0; i < h->m_capacity; i++ )
            {
                (++np)->link( np1 );
                this->m_alloc.construct<Field>( np );
                np1 = np;
            }
            h->m_p_free->link( np );
            return;
        }

    void finalize() throw()
        {
            UInt32 sz = head_size + ( this->m_head->m_capacity + 2 ) * node_size;
            this->m_alloc.deallocate( this->m_head.get() );
        }

    void initialize( UInt32 count ) throw()
        {
            UInt32 sz = head_size + ( count + 2 ) * node_size;
            this->m_head = TYPE<PHead>(this->m_alloc.allocate<UInt8>( sz ));
            PHead h = this->m_head.get();
            h->m_count = 0;
            h->m_capacity = count;
            PNode np = TYPE<PNode>(this->m_head.get() + 1);
            np->link( np );
            h->m_p_used = np;
            h->m_p_free = ++np;
            PNode np1 = np;
            for ( UInt32 i = 0; i < count; i++ )
            {
                (++np)->link( np1 );
                this->m_alloc.construct<Field>( np );
                np1 = np;
            }
            h->m_p_free->link( np );
        }

    bool q_empty( PNode q ) const throw()
        {
            return ( q->next() == q );
        }

private:
    Allocator m_alloc;
    Pointer<_Head> m_head;
};

} // namespace Common

#endif // H_GTABLE
