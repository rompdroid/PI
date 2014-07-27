///////////////////////////////////////////////////////////////////////////////////////////////////
//
// stree.h
// Copyright (c) 2009 by Xie Yun. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef H_STREE
#define H_STREE

#include "gtable.h"

namespace Common
{
// STree
//    static tree
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _field,
    unsigned int _col,
    class _alloc = default_alloc >
class STree
    : protected GTable<GNode<_field, _col>, _alloc>
{
public:
    typedef STree<_field, _col, _alloc> Self;
    typedef GNode<_field, _col> _Node;
    typedef GTable<_Node, _alloc> Super;
    typedef _field Field;
    typedef _alloc Allocator;
    enum
    {
        columns = _col
    };

protected:
    typedef typename _Node::Nodeptr Nodeptr;
    typedef typename _Node::PNode PNode;
    typedef Field * PField;

    /*class _Iterator
        : public Nodeptr
    {
    public:
        typedef Nodeptr Super;
        typedef typename Super::pointer pointer;
        typedef typename Super::const_pointer const_pointer;

        _Iterator( pointer p = 0 )
            : Super( p )
            {}

        _Iterator( const _Iterator & rhs )
            : Super( rhs )
            {};

        Field & operator* () const
            {
                if ( !*this )
                    throw Exception( "bad_pointer" );

                return *this->get();
            }

        _Iterator & operator= ( pointer rhs )
            {
                this->Super::operator=( rhs );
                return *this;
            }

        _Iterator & operator= ( const_pointer rhs )
            {
                this->Super::operator=( rhs );
                return *this;
            }

        _Iterator & operator= ( const _Iterator & rhs )
            {
                *this = rhs.get();
                return *this;
            }

        bool operator== ( const _Iterator & rhs ) const throw()
            {
                return ( *this == rhs.get() );
            }

        bool operator!= ( const _Iterator & rhs ) const throw()
            {
                return !( *this == rhs );
            }

        template< unsigned int _fn, typename _t >
        pointer exists( const _t & tar ) const throw()
            {
                if ( !*this )
                    return 0;

                pointer p = this->get();
                while ( p && ( *ptr != tar ) )
                {
                    p = p->next( _fn );
                }
                return p;
            }

        template< unsigned int _fn >
        _Iterator & next() throw()
            {
                if ( *this )
                {
                    *this = (*this)->next<_fn>();
                }
                return *this;
            }

        template< unsigned int _fn >
        _Iterator & prev() throw()
            {
                if ( *this )
                {
                    *this = (*this)->prev<_fn>();
                }
                return *this;
            }

        //bool ended() const throw()
        //    {
        //        return ( !*this );
        //    }

        //template< unsigned int _fn >
        //pointer & get() const throw()
        //    {
        //        return (*this)->m_next[_fn];
        //    }

        //_Iterator & next( unsigned int fn ) throw()
        //    {
        //        if ( !this->ended() )
        //        {
        //            *this = (*this)->m_next[fn];
        //        }
        //        return *this;
        //    }

        //template< unsigned int _fn >
        //_Iterator & prev( const _Iterator & it ) throw()
        //    {
        //        if ( this->ended() )
        //            return *this;

        //        pointer t_ptr = it.get();
        //        pointer ptr = this->get();
        //        while ( ptr && t_ptr != ptr->m_next[_fn] )
        //        {
        //            ptr = ptr->m_next[_fn];
        //        }
        //        *this = ptr;
        //        return *this;
        //    }

        //template< unsigned int _fn >
        //_Iterator & prev( unsigned int field, const Field & f ) throw()
        //    {
        //        if ( this->ended() )
        //            return *this;

        //        pointer ptr = this->get();
        //        while ( ptr )
        //        {
        //            pointer p = ptr->m_next[_fn];
        //            if ( p && p->m_field[field] == f )
        //                break;

        //            ptr = p;
        //        }
        //        *this = ptr;
        //        return *this;
        //    }
    };*/

public:
    typedef typename PNode pointer;
    typedef typename Nodeptr node_pointer;
    typedef typename PField field_pointer;
    typedef typename Super::pointer super_pointer;

    STree( UInt32 count = 0, const Allocator & alloc = Allocator() ) throw()
        : Super( count, alloc )
        {}

    STree( const Self & rhs ) throw()
        : Super( rhs )
        {}

    ~STree() throw()
        {}

    using Super::allocator;
    using Super::capacity;
    using Super::count;
    using Super::begin;
    using Super::end;
    using Super::exists;
    using Super::insert;
    using Super::rinsert;

    PNode erase( PField node ) throw()
        {
            PNode np = this->Super::erase( TYPE<PNode>(node) );
            if ( np )
            {
                this->allocator().destruct<_Node>( node );
            }
            return np;
        }

    template< unsigned int _fn >
    PNode erase( PNode node ) throw()
        {
            PNode np = node->next<_fn>();
            node->unlink<_fn>();
            return np;
        }

    template< unsigned int _fn >
    PNode fetch( PNode _where, Int32 at ) const throw()
    {
        if ( at == at_first )
            return _where;

        PNode np = _where;
        if ( at < at_first )
        {
            for ( np = _where; at; at++ )
            {
                np = np->prior<_fn>();
            }
        }
        else
        {
            for ( np = _where; at; at-- )
            {
                np = np->next<_fn>();
            }
        }
        return np;
    }

    template< unsigned int _fn >
    void insert( PNode _where, PNode node ) throw() // insert before _where
        {
            node->link<_fn>( _where->prior<_fn>() );
            _where->link<_fn>( node );
        }

    template< unsigned int _fn >
    void rinsert( PNode _where, PNode node ) throw() // insert after _where
        {
            _where->next<_fn>()->link<_fn>( node );
            node->link<_fn>( _where );
        }
};

// STree: columns = 1
template< class _field,
    class _alloc >
class STree<_field, 1, _alloc>
    : protected GTable<GNode<_field, 1>, _alloc>
{
public:
    typedef STree<_field, 1, _alloc> Self;
    typedef GNode<_field, 1> _Node;
    typedef GTable<_Node, _alloc> Super;
    typedef _field Field;
    typedef _alloc Allocator;

protected:
    typedef typename _Node::Nodeptr Nodeptr;
    typedef typename _Node::PNode PNode;
    typedef Field * PField;

public:
    typedef typename PNode pointer;
    typedef typename Nodeptr node_pointer;
    typedef typename PField field_pointer;
    typedef typename Super::pointer super_pointer;

    STree( UInt32 count = 0, const Allocator & alloc = Allocator() ) throw()
        : Super( count, alloc )
        {}

    STree( const Self & rhs ) throw()
        : Super( rhs )
        {}

    ~STree() throw()
        {}

    using Super::allocator;
    using Super::capacity;
    using Super::count;
    using Super::begin;
    using Super::end;
    using Super::exists;
    using Super::insert;
    using Super::rinsert;

    PNode erase( PField node ) throw()
        {
            PNode np = this->Super::erase( TYPE<PNode>(node) );
            if ( np )
            {
                this->allocator().destroy<_Node>( node );
            }
            return np;
        }

    void insert( PNode _where, PNode node ) throw() // insert before _where
        {
            node->link( _where->prior() );
            _where->link( node );
        }

    void rinsert( PNode _where, PNode node ) throw() // insert after _where
        {
            _where->next()->link( node );
            node->link( _where );
        }
};

} // namespace Common

#endif // H_STREE
