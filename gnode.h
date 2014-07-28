///////////////////////////////////////////////////////////////////////////////////////////////////
//
// gnode.h
// Copyright (c) 2009 by Xie Yun.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef H_GNODE
#define H_GNODE

#include "../pointer.h"

namespace Common
{

enum
{
    at_first = 0,
    at_last = -1
};

// GNode 
//    linked node abstraction
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _type >
struct LinkNode
{
    typedef _type _Node;
    typedef Pointer<_Node> Nodeptr;
    typedef typename Nodeptr::pointer PNode;

    Nodeptr m_next;
    Nodeptr m_prior;
};


template< class _field, unsigned int _col >
class GNode
    : public _field
{
public:
    typedef GNode<_field, _col> Self;
    typedef LinkNode<Self> _Link;
    typedef typename _Link::Nodeptr Nodeptr;
    typedef typename _Link::PNode PNode;
    typedef _field Field;
    enum
    {
        columns = _col,
    };

public:
    GNode() throw()
        {
            this->mlink<columns>( this );
        }

    template< typename _Ty >
    GNode( const _Ty & v )
        : Field( v )
        {
            this->mlink<columns>( this );
        }

    ~GNode() throw()
        {
            this->unmlink<columns>();
        }

    template< unsigned int _fn >
    void link( PNode p )
        {
            this->m_link[_fn].m_prior = p;
            p->m_link[_fn].m_next = this;
        }

    template< unsigned int _fn >
    void unlink()
        {
            _Link & _l = this->m_link[_fn];
            _l.m_prior->m_link[_fn].m_next = _l.m_next;
            _l.m_next->m_link[_fn].m_prior = _l.m_prior;
            _l.m_next = this;
            _l.m_prior = this;
        }

    template< unsigned int _n >
    void mlink( PNode p )
        {
            this->mlink<_n-1>( p );
            this->link<_n-1>( p );
        }

    template<>
    void mlink<0>( PNode p )
        {
            this->link<0>( p );
        }

    template< unsigned int _n >
    void mlink( PNode * lp )
        {
            this->mlink<_n-1>( lp );
            this->link<_n-1>( lp[_n-1] );
        }

    template<>
    void mlink<0>( PNode * lp )
        {
            this->link<0>( lp[0] );
        }

    template< unsigned int _n >
    void unmlink()
        {
            this->unmlink<_n-1>();
            this->unlink<_n-1>();
        }

    template<>
    void unmlink<0>()
        {
            this->unlink<0>();
        }

    template< unsigned int _fn >
    typename PNode next()
        {
            return this->m_link[_fn].m_next.get();
        }

    template< unsigned int _fn >
    typename PNode prior()
        {
            return this->m_link[_fn].m_prior.get();
        }

protected:
    _Link m_link[columns];
};

template< class _field >
class GNode<_field, 1>
    : public _field
{
public:
    typedef GNode<_field, 1> Self;
    typedef LinkNode<Self> _Link;
    typedef typename _Link::Nodeptr Nodeptr;
    typedef typename _Link::PNode PNode;
    typedef _field Field;

public:
    GNode() throw()
        {
            this->link( this );
        }

    template< typename _Ty >
    GNode( const _Ty & v )
        : Field( v )
        {
            this->link( this );
        }

    ~GNode() throw()
        {
            this->unlink();
        }

    void link( PNode p )
        {
            this->m_link.m_prior = p;
            p->m_link.m_next = this;
        }

    void unlink()
        {
            _Link & _l = this->m_link;
            _l.m_prior->m_link.m_next = _l.m_next;
            _l.m_next->m_link.m_prior = _l.m_prior;
            _l.m_next = this;
            _l.m_prior = this;
        }

    typename PNode next()
        {
            return this->m_link.m_next.get();
        }

    typename PNode prior()
        {
            return this->m_link.m_prior.get();
        }

protected:
    _Link m_link;
};

} // namespace Common

#endif // H_GNODE
